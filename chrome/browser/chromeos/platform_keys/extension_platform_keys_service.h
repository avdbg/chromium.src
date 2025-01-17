// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class StateStore;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace policy {
class PolicyService;
}

namespace chromeos {

class ExtensionPlatformKeysService : public KeyedService {
 public:
  // The SelectDelegate is used to select a single certificate from all
  // certificates matching a request (see SelectClientCertificates). E.g. this
  // can happen by exposing UI to let the user select.
  class SelectDelegate {
   public:
    using CertificateSelectedCallback = base::OnceCallback<void(
        const scoped_refptr<net::X509Certificate>& selection)>;

    SelectDelegate();
    virtual ~SelectDelegate();

    // Called on an interactive SelectClientCertificates call with the list of
    // matching certificates, |certs|.
    // The certificate passed to |callback| will be forwarded to the
    // calling extension and the extension will get unlimited sign permission
    // for this cert. By passing null to |callback|, no cert will be selected.
    // Must eventually call |callback| or be destructed. |callback| must not be
    // called after this delegate is destructed.
    // |web_contents| and |context| provide the context in which the
    // certificates were requested and are not null.
    virtual void Select(const std::string& extension_id,
                        const net::CertificateList& certs,
                        CertificateSelectedCallback callback,
                        content::WebContents* web_contents,
                        content::BrowserContext* context) = 0;

   private:
    DISALLOW_ASSIGN(SelectDelegate);
  };

  // Stores registration information in |state_store|, i.e. for each extension
  // the list of public keys that are valid to be used for signing. See
  // |ExtensionKeyPermissionsService| for more details.
  // |browser_context| and |state_store| must not be null and outlive this
  // object.
  explicit ExtensionPlatformKeysService(
      bool profile_is_managed,
      PrefService* profile_prefs,
      policy::PolicyService* profile_policies,
      content::BrowserContext* browser_context,
      extensions::StateStore* state_store);

  ~ExtensionPlatformKeysService() override;

  // Sets the delegate which will be used for interactive
  // SelectClientCertificates calls.
  void SetSelectDelegate(std::unique_ptr<SelectDelegate> delegate);

  // If the generation was successful, |public_key_spki_der| will contain the
  // DER encoding of the SubjectPublicKeyInfo of the generated key. If it
  // failed, |public_key_spki_der| will be empty.
  using GenerateKeyCallback =
      base::OnceCallback<void(const std::string& public_key_spki_der,
                              platform_keys::Status status)>;

  // Generates an RSA key pair with |modulus_length_bits| and registers the key
  // to allow a single sign operation by the given extension. |token_id|
  // specifies the token to store the key pair on. If the generation was
  // successful, |callback| will be invoked with the resulting public key. If it
  // failed, the resulting public key will be empty. Will only call back during
  // the lifetime of this object.
  void GenerateRSAKey(platform_keys::TokenId token_id,
                      unsigned int modulus_length_bits,
                      const std::string& extension_id,
                      GenerateKeyCallback callback);

  // Generates an EC key pair with |named_curve| and registers the key to allow
  // a single sign operation by the given extension. |token_id| specifies the
  // token to store the key pair on. If the generation was successful,
  // |callback| will be invoked with the resulting public key. If it failed, the
  // resulting public key will be empty. Will only call back during the lifetime
  // of this object.
  void GenerateECKey(platform_keys::TokenId token_id,
                     const std::string& named_curve,
                     const std::string& extension_id,
                     GenerateKeyCallback callback);

  // Gets the current profile using the BrowserContext object and returns
  // whether the current profile is a sign in profile with
  // ProfileHelper::IsSigninProfile.
  bool IsUsingSigninProfile();

  // If signing was successful, |signature| will contain the signature. If it
  // failed, |signature| will be empty.
  using SignCallback = base::OnceCallback<void(const std::string& signature,
                                               platform_keys::Status status)>;

  // Digests |data|, applies PKCS1 padding if specified by |hash_algorithm| and
  // chooses the signature algorithm according to |key_type| and signs the data
  // with the private key matching |public_key_spki_der|. If a |token_id|
  // is provided and the key is not found in that token, the operation aborts.
  // If |token_id| is not provided (nullopt), all tokens available to the caller
  // will be considered while searching for the key.
  // If the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts. If signing was successful, |callback| will be
  // invoked with the signature. If it failed, the resulting signature will be
  // empty. Will only call back during the lifetime of this object.
  void SignDigest(base::Optional<platform_keys::TokenId> token_id,
                  const std::string& data,
                  const std::string& public_key_spki_der,
                  platform_keys::KeyType key_type,
                  platform_keys::HashAlgorithm hash_algorithm,
                  const std::string& extension_id,
                  SignCallback callback);

  // Applies PKCS1 padding and afterwards signs the data with the private key
  // matching |public_key_spki_der|. |data| is not digested. If a |token_id|
  // is provided and the key is not found in that token, the operation aborts.
  // If |token_id| is not provided (nullopt), all available tokens to the caller
  // will be considered while searching for the key. The size of |data| (number
  // of octets) must be smaller than k - 11, where k is the key size in octets.
  // If the extension does not have permissions for signing with this key, the
  // operation aborts. In case of a one time permission (granted after
  // generating the key), this function also removes the permission to prevent
  // future signing attempts. If signing was successful, |callback| will be
  // invoked with the signature. If it failed, the resulting signature will be
  // empty. Will only call back during the lifetime of this object.
  void SignRSAPKCS1Raw(base::Optional<platform_keys::TokenId> token_id,
                       const std::string& data,
                       const std::string& public_key_spki_der,
                       const std::string& extension_id,
                       SignCallback callback);

  // If the certificate request could be processed successfully, |matches| will
  // contain the list of matching certificates (maybe empty). If an error
  // occurred, |matches| will be null.
  using SelectCertificatesCallback =
      base::OnceCallback<void(std::unique_ptr<net::CertificateList> matches,
                              platform_keys::Status status)>;

  // Returns a list of certificates matching |request|.
  // 1) all certificates that match the request (like being rooted in one of the
  // give CAs) are determined.
  // 2) if |client_certificates| is not null, drops all certificates that are
  // not elements of |client_certificates|,
  // 3) if |interactive| is true, the currently set SelectDelegate is used to
  // select a single certificate from these matches
  // which will the extension will also be granted access to.
  // 4) only certificates, that the extension has unlimited sign permission for,
  // will be returned.
  // If selection was successful, |callback| will be invoked with these
  // certificates. If it failed, the resulting certificate list will be empty
  // and an error status will be returned. Will only call back during the
  // lifetime of this object. |web_contents| must not be null.
  void SelectClientCertificates(
      const platform_keys::ClientCertificateRequest& request,
      std::unique_ptr<net::CertificateList> client_certificates,
      bool interactive,
      const std::string& extension_id,
      SelectCertificatesCallback callback,
      content::WebContents* web_contents);

 private:
  class GenerateRSAKeyTask;
  class GenerateECKeyTask;
  class GenerateKeyTask;
  class SelectTask;
  class SignTask;
  class Task;

  // Starts |task| eventually. To ensure that at most one |Task| is running at a
  // time, it queues |task| for later execution if necessary.
  void StartOrQueueTask(std::unique_ptr<Task> task);

  // Must be called after |task| is done. |task| will be invalid after this
  // call. This must not be called for any but the task that ran last. If any
  // other tasks are queued (see StartOrQueueTask()), it will start the next
  // one.
  void TaskFinished(Task* task);

  // Callback used by |GenerateRSAKey|.
  // If the key generation was successful, registers the generated public key
  // for the given extension. If any error occurs during key generation or
  // registration, calls |callback| with an error status. Otherwise, on success,
  // calls |callback| with the public key.
  void GeneratedKey(const std::string& extension_id,
                    const GenerateKeyCallback& callback,
                    const std::string& public_key_spki_der,
                    platform_keys::Status status);

  content::BrowserContext* const browser_context_ = nullptr;
  platform_keys::PlatformKeysService* const platform_keys_service_ = nullptr;
  platform_keys::KeyPermissionsService* const key_permissions_service_ =
      nullptr;
  std::unique_ptr<SelectDelegate> select_delegate_;
  base::queue<std::unique_ptr<Task>> tasks_;
  base::WeakPtrFactory<ExtensionPlatformKeysService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionPlatformKeysService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_EXTENSION_PLATFORM_KEYS_SERVICE_H_
