// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/keymaster/cert_store_bridge.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/keymaster.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class is responsible for providing a KeymasterServer proxy by
// bootstrapping a mojo connection with the arc-keymasterd daemon. The mojo
// connection is bootstrapped lazily during the first call to GetServer. Chrome
// has no further involvement once the KeymasterServer proxy has been forwarded
// to the KeymasterInstance in ARC.
class ArcKeymasterBridge : public KeyedService, public mojom::KeymasterHost {
 public:
  using mojom::KeymasterHost::GetServerCallback;
  using UpdatePlaceholderKeysCallback = base::OnceCallback<void(bool)>;

  // Returns singleton instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcKeymasterBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcKeymasterBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcKeymasterBridge() override;

  // Return the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Update the list of placeholder keys to be instlaled in arc-keymasterd.
  //
  // Made virtual for override in tests.
  virtual void UpdatePlaceholderKeys(
      std::vector<keymaster::mojom::ChromeOsKeyPtr> keys,
      UpdatePlaceholderKeysCallback callback);

  // KeymasterHost mojo interface.
  void GetServer(GetServerCallback callback) override;

 private:
  using BootstrapMojoConnectionCallback = base::OnceCallback<void(bool)>;

  void BootstrapMojoConnection(BootstrapMojoConnectionCallback callback);
  void OnBootstrapMojoConnection(BootstrapMojoConnectionCallback callback,
                                 bool bootstrapResult);
  void UpdatePlaceholderKeysAfterBootstrap(
      std::vector<keymaster::mojom::ChromeOsKeyPtr> keys,
      UpdatePlaceholderKeysCallback callback,
      bool bootstrapResult);
  void GetServerAfterBootstrap(GetServerCallback callback,
                               bool bootstrapResult);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // Points to a proxy bound to the implementation in arc-keymasterd.
  mojo::Remote<mojom::KeymasterServer> keymaster_server_proxy_;

  // Points to the host implementation in Chrome, used to interact with the
  // arc-keymasterd daemon.
  std::unique_ptr<keymaster::CertStoreBridge> cert_store_bridge_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcKeymasterBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcKeymasterBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_KEYMASTER_ARC_KEYMASTER_BRIDGE_H_
