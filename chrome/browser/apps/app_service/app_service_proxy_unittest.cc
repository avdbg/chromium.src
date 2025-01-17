// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

class AppServiceProxyTest : public testing::Test {
 protected:
  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;

  class FakeIconLoader : public apps::IconLoader {
   public:
    void FlushPendingCallbacks() {
      for (auto& callback : pending_callbacks_) {
        auto iv = apps::mojom::IconValue::New();
        iv->icon_type = apps::mojom::IconType::kUncompressed;
        iv->uncompressed =
            gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
        iv->is_placeholder_icon = false;

        std::move(callback).Run(std::move(iv));
        num_inner_finished_callbacks_++;
      }
      pending_callbacks_.clear();
    }

    int NumInnerFinishedCallbacks() { return num_inner_finished_callbacks_; }
    int NumPendingCallbacks() { return pending_callbacks_.size(); }

   private:
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override {
      return apps::mojom::IconKey::New(0, 0, 0);
    }

    std::unique_ptr<Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override {
      if (icon_type == apps::mojom::IconType::kUncompressed) {
        pending_callbacks_.push_back(std::move(callback));
      }
      return nullptr;
    }

    int num_inner_finished_callbacks_ = 0;
    std::vector<apps::mojom::Publisher::LoadIconCallback> pending_callbacks_;
  };

  UniqueReleaser LoadIcon(apps::IconLoader* loader, const std::string& app_id) {
    static constexpr auto app_type = apps::mojom::AppType::kWeb;
    static constexpr auto icon_type = apps::mojom::IconType::kUncompressed;
    static constexpr int32_t size_hint_in_dip = 1;
    static bool allow_placeholder_icon = false;

    return loader->LoadIcon(app_type, app_id, icon_type, size_hint_in_dip,
                            allow_placeholder_icon,
                            base::BindOnce(&AppServiceProxyTest::OnLoadIcon,
                                           base::Unretained(this)));
  }

  void OverrideAppServiceProxyInnerIconLoader(apps::AppServiceProxy* proxy,
                                              apps::IconLoader* icon_loader) {
    proxy->OverrideInnerIconLoaderForTesting(icon_loader);
  }

  void OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
    num_outer_finished_callbacks_++;
  }

  int NumOuterFinishedCallbacks() { return num_outer_finished_callbacks_; }

  int num_outer_finished_callbacks_ = 0;

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppServiceProxyTest, IconCache) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCache code, see icon_cache_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCache but also other IconLoader filters, such as an IconCoalescer.

  apps::AppServiceProxy proxy(nullptr);
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(&proxy, &fake);

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c0 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(1, NumOuterFinishedCallbacks());

  // The next LoadIcon call should be a cache hit.
  UniqueReleaser c1 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // Destroy the IconLoader::Releaser's, clearing the cache.
  c0.reset();
  c1.reset();

  // The next LoadIcon call should be a cache miss.
  UniqueReleaser c2 = LoadIcon(&proxy, "cromulent");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(1, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(2, NumOuterFinishedCallbacks());

  // After a cache miss, manually trigger the inner callback.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(3, NumOuterFinishedCallbacks());
}

TEST_F(AppServiceProxyTest, IconCoalescer) {
  // This is mostly a sanity check. For an isolated, comprehensive unit test of
  // the IconCoalescer code, see icon_coalescer_unittest.cc.
  //
  // This tests an AppServiceProxy as a 'black box', which uses an
  // IconCoalescer but also other IconLoader filters, such as an IconCache.

  apps::AppServiceProxy proxy(nullptr);
  FakeIconLoader fake;
  OverrideAppServiceProxyInnerIconLoader(&proxy, &fake);

  // Issue 4 LoadIcon requests, 2 after de-duplication.
  UniqueReleaser a0 = LoadIcon(&proxy, "avocet");
  UniqueReleaser a1 = LoadIcon(&proxy, "avocet");
  UniqueReleaser b2 = LoadIcon(&proxy, "brolga");
  UniqueReleaser a3 = LoadIcon(&proxy, "avocet");
  EXPECT_EQ(2, fake.NumPendingCallbacks());
  EXPECT_EQ(0, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(0, NumOuterFinishedCallbacks());

  // Resolve their responses.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Issue another request, that triggers neither IconCache nor IconCoalescer.
  UniqueReleaser c4 = LoadIcon(&proxy, "curlew");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Destroying the IconLoader::Releaser shouldn't affect the fact that there's
  // an in-flight "curlew" request to the FakeIconLoader.
  c4.reset();
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Issuing another "curlew" request should coalesce with the in-flight one.
  UniqueReleaser c5 = LoadIcon(&proxy, "curlew");
  EXPECT_EQ(1, fake.NumPendingCallbacks());
  EXPECT_EQ(2, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(4, NumOuterFinishedCallbacks());

  // Resolving the in-flight request to the inner IconLoader, |fake|, should
  // resolve the two coalesced requests to the outer IconLoader, |proxy|.
  fake.FlushPendingCallbacks();
  EXPECT_EQ(0, fake.NumPendingCallbacks());
  EXPECT_EQ(3, fake.NumInnerFinishedCallbacks());
  EXPECT_EQ(6, NumOuterFinishedCallbacks());
}

class GuestAppServiceProxyTest : public AppServiceProxyTest,
                                 public ::testing::WithParamInterface<bool> {
 public:
  GuestAppServiceProxyTest() {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GuestAppServiceProxyTest, ProxyAccessPerProfile) {
  TestingProfile::Builder profile_builder;

  // We expect an App Service in a regular profile.
  auto profile = profile_builder.Build();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile.get());
  EXPECT_TRUE(proxy);

  // We expect the same App Service in the incognito profile branched from that
  // regular profile.
  // TODO(https://crbug.com/1122463): this should be nullptr once we address all
  // incognito access to the App Service.
  TestingProfile::Builder incognito_builder;
  auto* incognito_proxy = apps::AppServiceProxyFactory::GetForProfile(
      incognito_builder.BuildIncognito(profile.get()));
  EXPECT_EQ(proxy, incognito_proxy);

  // We expect a different App Service in the Guest Session profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
  auto* guest_proxy =
      apps::AppServiceProxyFactory::GetForProfile(guest_profile.get());
  EXPECT_TRUE(guest_proxy);
  EXPECT_NE(guest_proxy, proxy);
}

TEST_P(GuestAppServiceProxyTest, RedirectInIncognitoProxyAccessPerProfile) {
  TestingProfile::Builder profile_builder;

  // We expect an App Service in a regular profile.
  auto profile = profile_builder.Build();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile.get());
  EXPECT_TRUE(proxy);

  // We get the same App Service using GetForProfileRedirectInIncognito.
  auto* redirected_proxy =
      apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(
          profile.get());
  EXPECT_EQ(proxy, redirected_proxy);

  // We expect the same App Service in the incognito profile branched from that
  // regular profile.
  TestingProfile::Builder incognito_builder;
  auto* incognito_proxy =
      apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(
          incognito_builder.BuildIncognito(profile.get()));
  EXPECT_EQ(proxy, incognito_proxy);

  // We expect a different (but still valid) App Service in the Guest Session
  // profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();
  auto* guest_proxy =
      apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(
          guest_profile.get());
  EXPECT_TRUE(guest_proxy);
  EXPECT_NE(guest_proxy, proxy);
}

INSTANTIATE_TEST_SUITE_P(AllGuestTypes,
                         GuestAppServiceProxyTest,
                         /*is_ephemeral=*/testing::Bool());
