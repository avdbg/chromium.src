// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/notification_test_utils_mac.h"

@implementation FakeUNNotification
@synthesize request = _request;
- (void)dealloc {
  [_request release];
  [super dealloc];
}
@end

@implementation FakeUNNotificationResponse
@synthesize notification = _notification;
@synthesize actionIdentifier = _actionIdentifier;
- (void)dealloc {
  [_notification release];
  [_actionIdentifier release];
  [super dealloc];
}
@end

@implementation FakeUNNotificationSettings
@synthesize alertStyle = _alertStyle;
@synthesize authorizationStatus = _authorizationStatus;
@end

@implementation FakeUNUserNotificationCenter {
  base::scoped_nsobject<FakeUNNotificationSettings> _settings;
  base::scoped_nsobject<NSMutableDictionary> _notifications;
  base::scoped_nsobject<NSSet<UNNotificationCategory*>> _categories;
}

- (instancetype)init {
  if ((self = [super init])) {
    _settings.reset([[FakeUNNotificationSettings alloc] init]);
    _notifications.reset([[NSMutableDictionary alloc] init]);
    _categories.reset([[NSSet alloc] init]);
  }
  return self;
}

- (void)setDelegate:(id<UNUserNotificationCenterDelegate>)delegate {
}

- (void)removeAllDeliveredNotifications {
  [_notifications removeAllObjects];
}

- (void)setNotificationCategories:(NSSet<UNNotificationCategory*>*)categories {
  _categories.reset([categories copy]);
}

- (void)replaceContentForRequestWithIdentifier:(NSString*)requestIdentifier
                            replacementContent:
                                (UNMutableNotificationContent*)content
                             completionHandler:
                                 (void (^)(NSError* _Nullable error))
                                     notificationDelivered {
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:requestIdentifier
                                           content:content
                                           trigger:nil];
  base::scoped_nsobject<FakeUNNotification> notification(
      [[FakeUNNotification alloc] init]);
  [notification setRequest:request];
  [_notifications setObject:notification forKey:[request identifier]];
  notificationDelivered(/*error=*/nil);
}

- (void)addNotificationRequest:(UNNotificationRequest*)request
         withCompletionHandler:(void (^)(NSError* error))completionHandler {
  base::scoped_nsobject<FakeUNNotification> notification(
      [[FakeUNNotification alloc] init]);
  [notification setRequest:request];
  [_notifications setObject:notification forKey:[request identifier]];
  completionHandler(/*error=*/nil);
}

- (void)getDeliveredNotificationsWithCompletionHandler:
    (void (^)(NSArray<UNNotification*>* notifications))completionHandler {
  completionHandler([_notifications allValues]);
}

- (void)getNotificationCategoriesWithCompletionHandler:
    (void (^)(NSSet<UNNotificationCategory*>* categories))completionHandler {
  completionHandler([[_categories copy] autorelease]);
}

- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(void (^)(BOOL granted, NSError* error))
                                            completionHandler {
  completionHandler(/*granted=*/YES, /*error=*/nil);
}

- (void)removeDeliveredNotificationsWithIdentifiers:
    (NSArray<NSString*>*)identifiers {
  [_notifications removeObjectsForKeys:identifiers];
}

- (void)getNotificationSettingsWithCompletionHandler:
    (void (^)(UNNotificationSettings* settings))completionHandler {
  completionHandler(static_cast<UNNotificationSettings*>(_settings.get()));
}

- (FakeUNNotificationSettings*)settings {
  return _settings.get();
}

- (NSArray<UNNotification*>* _Nonnull)notifications {
  return [_notifications allValues];
}

- (NSSet<UNNotificationCategory*>* _Nonnull)categories {
  return _categories.get();
}

@end

base::scoped_nsobject<FakeUNNotificationResponse>
CreateFakeUNNotificationResponse(NSDictionary* userInfo) {
  base::scoped_nsobject<UNMutableNotificationContent> content(
      [[UNMutableNotificationContent alloc] init]);
  [content setUserInfo:userInfo];

  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:@"identifier"
                                           content:content.get()
                                           trigger:nil];

  base::scoped_nsobject<FakeUNNotification> notification(
      [[FakeUNNotification alloc] init]);
  [notification setRequest:request];

  base::scoped_nsobject<FakeUNNotificationResponse> response(
      [[FakeUNNotificationResponse alloc] init]);
  [response setNotification:notification.get()];
  [response setActionIdentifier:UNNotificationDefaultActionIdentifier];

  return response;
}
