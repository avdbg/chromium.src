// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/receive_messages_express.h"

#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_http_result.h"
#include "chrome/browser/nearby_sharing/instantmessaging/constants.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/instantmessaging/token_fetcher.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

const base::TimeDelta kFastPathReadyTimeout =
    base::TimeDelta::FromMilliseconds(2500);

// Timeout for the receive messages stream, from when the stream first opens.
// This timeout applies to the Tachyon signaling process, so once we establish
// the peer-to-peer connection this stream and timeout will be canceled. There
// are other timeouts in the WebRTC medium that will cancel the signaling
// process sooner than 60s, so this is just a failsafe to make sure we clean up
// the ReceiveMessagesExpress if something goes wrong.
const base::TimeDelta kStreamTimeout = base::TimeDelta::FromSeconds(60);

// TODO(crbug.com/1123164) - Add nearby sharing policy when available.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("receive_messages_express", R"(
        semantics {
          sender: "ReceiveMessagesExpress"
          description:
            "Receives messages sent from another device via a Gaia "
            "authenticated Google messaging backend."
          trigger:
            "Peer uses any Chrome cross-device sharing feature and selects "
            "this devices to send the data to."
          data: "WebRTC session description protocol messages are exchanged "
            "between devices to set up a peer to peer connection as documented "
            "in https://tools.ietf.org/html/rfc4566 and "
            "https://www.w3.org/TR/webrtc/#session-description-model. No user "
            "data is sent in the request."
          destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature is only enabled for signed-in users who enable "
              "Nearby sharing"
            chrome_policy {
              BrowserSignin {
                policy_options {mode: MANDATORY}
                BrowserSignin: 0
              }
            }
          })");

base::Optional<NearbyShareHttpStatus> HttpStatusFromUrlLoader(
    const network::SimpleURLLoader* loader) {
  if (!loader)
    return base::nullopt;

  return NearbyShareHttpStatus(loader->NetError(), loader->ResponseInfo());
}

void LogReceiveResult(
    bool success,
    const base::Optional<NearbyShareHttpStatus>& http_status) {
  std::stringstream ss;
  ss << "Instant messaging receive express "
     << (success ? "succeeded." : "failed.");
  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.ReceiveExpress.Result", success);
  if (http_status) {
    ss << " HTTP status: " << *http_status;
    if (!success) {
      base::UmaHistogramSparse(
          "Nearby.Connections.InstantMessaging.ReceiveExpress.Result."
          "FailureReason",
          http_status->GetResultCodeForMetrics());
    }
  }

  if (success) {
    NS_LOG(INFO) << ss.str();
  } else {
    NS_LOG(ERROR) << ss.str();
  }
}

}  // namespace

// static
void ReceiveMessagesExpress::StartReceiveSession(
    const std::string& self_id,
    sharing::mojom::LocationHintPtr location_hint,
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    StartReceivingMessagesCallback callback,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  NS_LOG(INFO) << __func__ << ": self_id=" << self_id
               << ", location hint=" << location_hint->location
               << ", location format=" << location_hint->format;

  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request = BuildReceiveRequest(self_id, std::move(location_hint));

  auto receive_messages_express = base::WrapUnique(
      new ReceiveMessagesExpress(std::move(incoming_messages_listener),
                                 identity_manager, url_loader_factory));

  // Created a mojo pipe for the session that can be used to stop receiving.
  mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession> pending_remote;
  mojo::PendingReceiver<sharing::mojom::ReceiveMessagesSession>
      pending_receiver = pending_remote.InitWithNewPipeAndPassReceiver();

  receive_messages_express->StartReceivingMessages(request, std::move(callback),
                                                   std::move(pending_remote));

  mojo::MakeSelfOwnedReceiver(std::move(receive_messages_express),
                              std::move(pending_receiver));
}

ReceiveMessagesExpress::ReceiveMessagesExpress(
    mojo::PendingRemote<sharing::mojom::IncomingMessagesListener>
        incoming_messages_listener,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : incoming_messages_listener_(std::move(incoming_messages_listener)),
      token_fetcher_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      stream_parser_(
          base::BindRepeating(&ReceiveMessagesExpress::OnMessageReceived,
                              base::Unretained(this)),
          base::BindOnce(&ReceiveMessagesExpress::OnFastPathReady,
                         base::Unretained(this))) {}

ReceiveMessagesExpress::~ReceiveMessagesExpress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NS_LOG(VERBOSE) << __func__ << ": Receive messages session going down";

  fast_path_ready_timeout_timer_.Stop();

  if (start_receiving_messages_callback_) {
    std::move(start_receiving_messages_callback_)
        .Run(false, mojo::NullRemote());
  }
}

void ReceiveMessagesExpress::StartReceivingMessages(
    const chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesExpressRequest& request,
    StartReceivingMessagesCallback start_receiving_messages_callback,
    mojo::PendingRemote<sharing::mojom::ReceiveMessagesSession>
        pending_remote_for_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url_loader_);
  NS_LOG(VERBOSE) << "ReceiveMessagesExpress::StartReceivingMessages() called.";

  // Used to complete the initial mojo call once fast path is received.
  start_receiving_messages_callback_ =
      std::move(start_receiving_messages_callback);
  // This is the remote side of the self owned mojo pipe that will be returned
  // when completing start_receiving_messages_callback
  self_pending_remote_ = std::move(pending_remote_for_result);

  token_fetcher_.GetAccessToken(
      base::BindOnce(&ReceiveMessagesExpress::DoStartReceivingMessages,
                     weak_ptr_factory_.GetWeakPtr(), request));
}

void ReceiveMessagesExpress::DoStartReceivingMessages(
    const chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesExpressRequest& request,
    const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_ == nullptr);

  base::UmaHistogramBoolean(
      "Nearby.Connections.InstantMessaging.ReceiveExpress."
      "OAuthTokenFetchResult",
      !oauth_token.empty());
  if (oauth_token.empty()) {
    FailSessionAndDestruct("Auth token fetch failed");
    // |this| may be destroyed here.
    return;
  }

  NS_LOG(VERBOSE) << __func__
                  << ": OAuth token fetched; starting stream download";

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kInstantMessagingReceiveMessageAPI);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.AddHeaderFromString(
      base::StringPrintf(kAuthorizationHeaderFormat, oauth_token.c_str()));

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetTimeoutDuration(kStreamTimeout);
  url_loader_->AttachStringForUpload(request.SerializeAsString(),
                                     "application/x-protobuf");
  url_loader_->DownloadAsStream(url_loader_factory_.get(), this);

  // We are safe to use base::Unretained() here because if
  // ReceiveMessagesExpress is destroyed the timer will go out of scope first
  // which will cancel it.
  fast_path_ready_timeout_timer_.Start(
      FROM_HERE, kFastPathReadyTimeout,
      base::BindOnce(&ReceiveMessagesExpress::OnFastPathReadyTimeout,
                     base::Unretained(this)));
}

void ReceiveMessagesExpress::OnFastPathReadyTimeout() {
  NS_LOG(WARNING) << __func__;
  FailSessionAndDestruct("Timeout before receiving fast path ready");
  // |this| will be destroyed here.
  return;
}

void ReceiveMessagesExpress::StopReceivingMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_path_ready_timeout_timer_.Stop();

  // Cancel any pending calls into this object.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // This implicitly cancels the download stream. We intentionally don't call
  // OnComplete() when the other side calls StopReceivingMessages().
  url_loader_.reset();

  NS_LOG(VERBOSE) << __func__ << ": callback already invoked? "
                  << (start_receiving_messages_callback_ ? "no" : "yes");

  if (start_receiving_messages_callback_) {
    FailSessionAndDestruct(
        "StopReceivingMessages() called before fast path ready was received");
    // |this| destroyed here.
    return;
  }
}

void ReceiveMessagesExpress::OnDataReceived(base::StringPiece data,
                                            base::OnceClosure resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stream_parser_.Append(data);
  std::move(resume).Run();
}

void ReceiveMessagesExpress::OnComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_path_ready_timeout_timer_.Stop();
  base::Optional<NearbyShareHttpStatus> http_status =
      HttpStatusFromUrlLoader(url_loader_.get());

  NS_LOG(VERBOSE) << __func__ << ": success? " << (success ? "yes" : "no")
                  << ", start calback invoked? "
                  << (start_receiving_messages_callback_ ? "no" : "yes")
                  << ", net::Error " << url_loader_->NetError();

  if (start_receiving_messages_callback_) {
    LogReceiveResult(success, http_status);
    // If we have not called start_receiving_messages_callback_ yet, we
    // consider that a failure and need to complete the mojo call with a
    // failure.
    FailSessionAndDestruct("Download stream ended before fast path ready");
    // |this| will be destroyed here.
    return;
  } else {
    // Only call OnComplete() if the start callback has been invoked, meaning
    // the stream has opened and we have received "fast path ready".
    incoming_messages_listener_->OnComplete(success);
  }
}

void ReceiveMessagesExpress::OnRetry(base::OnceClosure start_retry) {
  NS_LOG(ERROR) << __func__ << ": retry is not implemented for the url_fetcher";
  NOTIMPLEMENTED();
}

void ReceiveMessagesExpress::OnFastPathReady() {
  NS_LOG(VERBOSE) << __func__;
  fast_path_ready_timeout_timer_.Stop();
  if (start_receiving_messages_callback_) {
    LogReceiveResult(/*success=*/true, /*http_status=*/base::nullopt);
    std::move(start_receiving_messages_callback_)
        .Run(true, std::move(self_pending_remote_));
  }
}

void ReceiveMessagesExpress::OnMessageReceived(const std::string& message) {
  NS_LOG(VERBOSE) << __func__ << ": message size: " << message.size();

  if (!incoming_messages_listener_) {
    NS_LOG(WARNING) << __func__ << ": no listener available to receive message";
    return;
  }

  incoming_messages_listener_->OnMessage(message);
}

void ReceiveMessagesExpress::FailSessionAndDestruct(const std::string reason) {
  // Cancel any pending calls into this object.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Explicitly stop any pending downloads if there are any.
  url_loader_.reset();
  if (start_receiving_messages_callback_) {
    // We don't give the remote in the callback because at this point
    // calling StopReceiveMessages won't do anything.
    std::move(start_receiving_messages_callback_)
        .Run(false, mojo::NullRemote());
  }

  NS_LOG(ERROR) << __func__
                << ": Terminating receive message express session: [" << reason
                << "]";
  // If we have not returned self_pending_remote_ to the caller, This will kill
  // the self-owned mojo pipe and implicitly destroy this object. If we have
  // given out this pending remote through |start_receiving_messages_callback_|,
  // the other side of the pipe controls the lifetime of this object and this
  // reset does nothing.
  self_pending_remote_.reset();
}
