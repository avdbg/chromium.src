// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/bindings/bindings_manager_cast.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

namespace {

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("chromecast/bindings/testdata");

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

class TitleChangeObserver : public CastWebContents::Observer {
 public:
  TitleChangeObserver() = default;
  ~TitleChangeObserver() override = default;

  // Spins a Runloop until the title of the page matches the |expected_title|
  // that have been set.
  void RunUntilTitleEquals(base::StringPiece expected_title) {
    expected_title_ = expected_title.as_string();
    // Spin the runloop until the expected conditions are met.
    if (current_title_ != expected_title_) {
      expected_title_ = expected_title.as_string();
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  // CastWebContents::Observer implementation:
  void UpdateTitle(const base::string16& title) override {
    // Resumes execution of RunUntilTitleEquals() if |title| matches
    // expectations.
    std::string title_utf8 = base::UTF16ToUTF8(title);
    current_title_ = title_utf8;
    if (!quit_closure_.is_null() && current_title_ == expected_title_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  std::string current_title_;
  std::string expected_title_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TitleChangeObserver);
};

// =============================================================================
// Mocks
// =============================================================================
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(CloseContents, void(content::WebContents* source));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

class MockCastWebContentsDelegate
    : public base::SupportsWeakPtr<MockCastWebContentsDelegate>,
      public CastWebContents::Delegate {
 public:
  MockCastWebContentsDelegate() {}
  ~MockCastWebContentsDelegate() override = default;

  MOCK_METHOD2(InnerContentsCreated,
               void(CastWebContents* inner_contents,
                    CastWebContents* outer_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsDelegate);
};

}  // namespace

// =============================================================================
// Test class
// =============================================================================
class BindingsManagerCastBrowserTest : public content::BrowserTestBase {
 protected:
  BindingsManagerCastBrowserTest() = default;
  ~BindingsManagerCastBrowserTest() override = default;

  void SetUp() final {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    BrowserTestBase::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) final {
    command_line->AppendSwitchASCII(switches::kTestType, "browser");
  }

  void PreRunTestOnMainThread() override {
    // Pump startup related events.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop().RunUntilIdle();

    metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
    content::WebContents::CreateParams create_params(
        shell::CastBrowserProcess::GetInstance()->browser_context(), nullptr);
    web_contents_ = content::WebContents::Create(create_params);
    web_contents_->SetDelegate(&mock_wc_delegate_);

    // CastWebContents::Delegate must be set for receiving PageStateChanged
    // event.
    CastWebContents::InitParams init_params;
    init_params.delegate = mock_cast_wc_delegate_.AsWeakPtr();
    init_params.is_root_window = true;

    cast_web_contents_ =
        std::make_unique<CastWebContentsImpl>(web_contents_.get(), init_params);
    title_change_observer_.Observe(cast_web_contents_.get());
    bindings_manager_ = std::make_unique<bindings::BindingsManagerCast>(
        cast_web_contents_.get());
  }

  void PostRunTestOnMainThread() override {
    cast_web_contents_.reset();
    web_contents_.reset();
    bindings_manager_.reset();
  }

  void StartTestServer() {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  NiceMock<MockCastWebContentsDelegate> mock_cast_wc_delegate_;
  NiceMock<MockWebContentsDelegate> mock_wc_delegate_;
  TitleChangeObserver title_change_observer_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastWebContentsImpl> cast_web_contents_;

  std::unique_ptr<bindings::BindingsManagerCast> bindings_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BindingsManagerCastBrowserTest);
};

// Handles connected ports from the NamedMessagePortConnector and
// provides a convenience methods for waiting for and then returning the port
// synchronously.
class MessagePortConnectionHandler {
 public:
  MessagePortConnectionHandler() {}
  ~MessagePortConnectionHandler() {}

  cast_api_bindings::Manager::MessagePortConnectedHandler GetConnectCallback() {
    return base::BindRepeating(&MessagePortConnectionHandler::OnConnect,
                               base::Unretained(this));
  }

  std::unique_ptr<cast_api_bindings::MessagePort> RunUntilPortConnected() {
    base::RunLoop run_loop;
    on_port_connected_ = run_loop.QuitClosure();
    run_loop.Run();
    return std::move(port_);
  }

 private:
  void OnConnect(std::unique_ptr<cast_api_bindings::MessagePort> port) {
    DCHECK(on_port_connected_);

    port_ = std::move(port);
    std::move(on_port_connected_).Run();
  }

  base::OnceClosure on_port_connected_;
  std::unique_ptr<cast_api_bindings::MessagePort> port_;
};

// =============================================================================
// Test cases
// =============================================================================
IN_PROC_BROWSER_TEST_F(BindingsManagerCastBrowserTest, EndToEnd) {
  // ===========================================================================
  // Test: Load BindingsManagerCast, ensure binding backend can receive a port
  // via BindingsManagerCast and the port is good to use.
  // Step 1: Create a TestBindingBackend object. TestBindingBackend will
  // register a PortHandler to BindingsManagerCast.
  // Step 2: Attach |bindings_manager_cast_| to |cast_web_contents_|, port
  // connector binding should be injected into |cast_web_contents_|.
  // Step 3: Load the test page, expected behaviours include:
  //  - BindingsManagerCast posts one end of MessagePort to the page.
  //    NamedMessagePort binding should be able to forward ports to native.
  //  - BindingManagerCast should successfully route a connected MessagePort to
  //    TestBindingBackend. This port is created by test page "connector.html".
  // Step 4: Verify that messages that are sent through the port are cached
  // before the port is not routed to native. And make sure TestBindingBackend
  // could use the |bindings_manager_cast_| provided port to send & receive
  // messages. Note: Messages should arrive in order.
  // ===========================================================================
  GURL test_url =
      content::GetFileUrlWithQuery(GetTestDataFilePath("connector.html"), "");

  MessagePortConnectionHandler connect_handler;
  bindings_manager_->RegisterPortHandler("hello",
                                         connect_handler.GetConnectCallback());

  // Load test page.
  constexpr char kTestPageTitle[] = "bindings";
  cast_web_contents_->LoadUrl(test_url);
  title_change_observer_.RunUntilTitleEquals(kTestPageTitle);

  auto message_port = connect_handler.RunUntilPortConnected();
  cast_api_bindings::TestMessagePortReceiver receiver;
  message_port->SetReceiver(&receiver);

  message_port->PostMessage("ping");

  // Test that message are received in order.
  receiver.RunUntilMessageCountEqual(3);
  EXPECT_EQ(receiver.buffer()[0].first, "early 1");
  EXPECT_EQ(receiver.buffer()[1].first, "early 2");
  EXPECT_EQ(receiver.buffer()[2].first, "ack ping");

  // Ensure that the MessagePort is dropped when navigating away.
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  receiver.RunUntilDisconnected();

  bindings_manager_->UnregisterPortHandler("hello");
}

}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_BROWSERTEST_H_
