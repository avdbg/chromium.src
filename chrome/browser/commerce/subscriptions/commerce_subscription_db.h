// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_DB_H_
#define CHROME_BROWSER_COMMERCE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_DB_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/commerce/subscriptions/commerce_subscription_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace commerce_subscription_db {
class CommerceSubscriptionContentProto;
}  // namespace commerce_subscription_db

template <typename T>
class ProfileProtoDB;

class CommerceSubscriptionDB {
 public:
  explicit CommerceSubscriptionDB(content::BrowserContext* browser_context);
  CommerceSubscriptionDB(const CommerceSubscriptionDB&) = delete;
  CommerceSubscriptionDB& operator=(const CommerceSubscriptionDB&) = delete;
  ~CommerceSubscriptionDB();

  // Save subscription for key.
  void Save(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const jint jtype,
            const base::android::JavaRef<jstring>& jtracking_id,
            const jint jmanagement_type,
            const jint jtracking_id_type,
            const jlong jtimestamp,
            const base::android::JavaRef<jobject>& jcallback);

  // Load subscription corresponding to key.
  void Load(JNIEnv* env,
            const base::android::JavaParamRef<jstring>& jkey,
            const base::android::JavaRef<jobject>& jcallback);

  // Load subscriptions whose keys have specific prefix.
  void LoadWithPrefix(JNIEnv* env,
                      const base::android::JavaParamRef<jstring>& jprefix,
                      const base::android::JavaRef<jobject>& jcallback);

  // Delete entry corresponding to key.
  void Delete(JNIEnv* env,
              const base::android::JavaParamRef<jstring>& jkey,
              const base::android::JavaRef<jobject>& jcallback);

  // Delete all entries in the database.
  void DeleteAll(JNIEnv* env, const base::android::JavaRef<jobject>& jcallback);

  // Destroy CommerceSubscriptionDB object.
  void Destroy(JNIEnv* env);

 private:
  ProfileProtoDB<commerce_subscription_db::CommerceSubscriptionContentProto>*
      proto_db_;
  base::WeakPtrFactory<CommerceSubscriptionDB> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_SUBSCRIPTIONS_COMMERCE_SUBSCRIPTION_DB_H_
