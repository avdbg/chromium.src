// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"

#include "chrome/test/base/testing_profile.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_view.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_route.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_types.h"

using media_router::MediaRoute;
using media_router::RouteControllerType;
using testing::_;

namespace {

MediaRoute CreateRoute(const std::string& route_id,
                       const std::string& source_id = "source_id") {
  media_router::MediaRoute route(route_id, media_router::MediaSource(source_id),
                                 "sink_id", "description", true, true);
  route.set_controller_type(media_router::RouteControllerType::kGeneric);
  return route;
}

class MockMediaNotificationController
    : public media_message_center::MediaNotificationController {
 public:
  MockMediaNotificationController() = default;
  ~MockMediaNotificationController() = default;

  MOCK_METHOD(void, ShowNotification, (const std::string& id));
  MOCK_METHOD(void, HideNotification, (const std::string& id));
  MOCK_METHOD(void, RemoveItem, (const std::string& id));
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override {
    return nullptr;
  }
  MOCK_METHOD(void,
              LogMediaSessionActionButtonPressed,
              (const std::string& id,
               media_session::mojom::MediaSessionAction action));
};

class MockMediaNotificationView
    : public media_message_center::MediaNotificationView {
 public:
  MOCK_METHOD1(SetExpanded, void(bool));
  MOCK_METHOD2(UpdateCornerRadius, void(int, int));
  MOCK_METHOD1(SetForcedExpandedState, void(bool*));
  MOCK_METHOD1(UpdateWithMediaSessionInfo,
               void(const media_session::mojom::MediaSessionInfoPtr&));
  MOCK_METHOD1(UpdateWithMediaMetadata,
               void(const media_session::MediaMetadata&));
  MOCK_METHOD1(
      UpdateWithMediaActions,
      void(const base::flat_set<media_session::mojom::MediaSessionAction>&));
  MOCK_METHOD1(UpdateWithMediaArtwork, void(const gfx::ImageSkia&));
  MOCK_METHOD1(UpdateWithFavicon, void(const gfx::ImageSkia&));
  MOCK_METHOD1(UpdateWithVectorIcon, void(const gfx::VectorIcon& vector_icon));
  MOCK_METHOD1(UpdateDeviceSelectorAvailability, void(bool availability));
};

class MockClosure {
 public:
  MOCK_METHOD0(Run, void());
};

}  // namespace

class CastMediaNotificationProviderTest : public testing::Test {
 public:
  void SetUp() override {
    notification_provider_ = std::make_unique<CastMediaNotificationProvider>(
        &profile_, &router_, &notification_controller_,
        base::BindRepeating(&MockClosure::Run,
                            base::Unretained(&items_changed_callback_)));
  }

  void TearDown() override { notification_provider_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CastMediaNotificationProvider> notification_provider_;
  MockMediaNotificationController notification_controller_;
  media_router::MockMediaRouter router_;
  MockClosure items_changed_callback_;
};

TEST_F(CastMediaNotificationProviderTest, AddAndRemoveRoute) {
  const std::string route_id = "route-id-1";
  MediaRoute route = CreateRoute(route_id);

  EXPECT_CALL(items_changed_callback_, Run());
  notification_provider_->OnRoutesUpdated({route}, {});
  testing::Mock::VerifyAndClearExpectations(&items_changed_callback_);
  EXPECT_EQ(1u, notification_provider_->GetActiveItemCount());
  EXPECT_NE(nullptr, notification_provider_->GetNotificationItem(route_id));

  EXPECT_CALL(items_changed_callback_, Run());
  notification_provider_->OnRoutesUpdated({}, {});
  testing::Mock::VerifyAndClearExpectations(&items_changed_callback_);
  EXPECT_EQ(0u, notification_provider_->GetActiveItemCount());
}

TEST_F(CastMediaNotificationProviderTest, UpdateRoute) {
  const std::string route_id = "route-id-1";
  MediaRoute route = CreateRoute(route_id);

  notification_provider_->OnRoutesUpdated({route}, {});
  auto* item = static_cast<CastMediaNotificationItem*>(
      notification_provider_->GetNotificationItem(route_id).get());
  MockMediaNotificationView view;
  item->SetView(&view);

  const std::string new_sink = "new sink";
  const std::string new_description = "new description";
  route.set_media_sink_name(new_sink);
  route.set_description(new_description);

  EXPECT_CALL(view, UpdateWithMediaMetadata(_))
      .WillOnce([&](const media_session::MediaMetadata& metadata) {
        const std::string separator = " \xC2\xB7 ";
        EXPECT_EQ(base::UTF8ToUTF16(new_description + separator + new_sink),
                  metadata.source_title);
      });
  notification_provider_->OnRoutesUpdated({route}, {});
}

TEST_F(CastMediaNotificationProviderTest, RoutesWithoutNotifications) {
  // These routes should not have notification items created for them.
  MediaRoute non_display_route = CreateRoute("route-1");
  non_display_route.set_for_display(false);
  MediaRoute no_controller_route = CreateRoute("route-2");
  no_controller_route.set_controller_type(RouteControllerType::kNone);
  MediaRoute multizone_member_route = CreateRoute("route-3", "cast:705D30C6");

  notification_provider_->OnRoutesUpdated(
      {non_display_route, no_controller_route, multizone_member_route}, {});
  EXPECT_EQ(0u, notification_provider_->GetActiveItemCount());
}

TEST_F(CastMediaNotificationProviderTest, DismissNotification) {
  const std::string route_id1 = "route-id-1";
  const std::string route_id2 = "route-id-2";
  MediaRoute route1 = CreateRoute(route_id1);
  MediaRoute route2 = CreateRoute(route_id2);
  notification_provider_->OnRoutesUpdated({route1}, {});
  EXPECT_EQ(1u, notification_provider_->GetActiveItemCount());

  notification_provider_->OnContainerDismissed(route_id1);
  EXPECT_EQ(0u, notification_provider_->GetActiveItemCount());

  // Adding another route should not bring back the dismissed notification.
  notification_provider_->OnRoutesUpdated({route1, route2}, {});
  EXPECT_EQ(1u, notification_provider_->GetActiveItemCount());
}
