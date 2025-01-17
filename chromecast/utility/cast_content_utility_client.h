// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_
#define CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/utility/content_utility_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromecast {
namespace shell {

class CastContentUtilityClient : public content::ContentUtilityClient {
 public:
  static std::unique_ptr<CastContentUtilityClient> Create();

  CastContentUtilityClient();

  // cast::ContentUtilityClient:
  bool HandleServiceRequestDeprecated(
      const std::string& service_name,
      mojo::ScopedMessagePipeHandle service_pipe) override;

  virtual bool HandleServiceRequest(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

 private:
  DISALLOW_COPY_AND_ASSIGN(CastContentUtilityClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_UTILITY_CAST_CONTENT_UTILITY_CLIENT_H_
