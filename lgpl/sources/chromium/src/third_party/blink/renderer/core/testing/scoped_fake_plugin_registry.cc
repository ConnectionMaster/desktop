// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

namespace {

class FakePluginRegistryImpl : public mojom::blink::PluginRegistry {
 public:
  static void Bind(mojo::ScopedMessagePipeHandle handle) {
    DEFINE_STATIC_LOCAL(FakePluginRegistryImpl, impl, ());
    impl.receivers_.Add(
        &impl,
        mojo::PendingReceiver<mojom::blink::PluginRegistry>(std::move(handle)));
  }

  // PluginRegistry
  void GetPlugins(bool refresh,
                  const scoped_refptr<const SecurityOrigin>& origin,
                  GetPluginsCallback callback) override {
    auto mime = mojom::blink::PluginMimeType::New();
    mime->mime_type = "application/pdf";
    mime->description = "pdf";

    auto plugin = mojom::blink::PluginInfo::New();
    plugin->name = "pdf";
    plugin->description = "pdf";
    plugin->filename = base::FilePath(FILE_PATH_LITERAL("pdf-files"));
    plugin->background_color = SkColorSetRGB(38, 38, 38);
    plugin->may_use_external_handler = true;
    plugin->mime_types.push_back(std::move(mime));

    Vector<mojom::blink::PluginInfoPtr> plugins;
    plugins.push_back(std::move(plugin));
    std::move(callback).Run(std::move(plugins));
  }

 private:
  mojo::ReceiverSet<PluginRegistry> receivers_;
};

}  // namespace

ScopedFakePluginRegistry::ScopedFakePluginRegistry() {
  Platform::Current()->GetBrowserInterfaceBrokerProxy()->SetBinderForTesting(
      mojom::blink::PluginRegistry::Name_,
      WTF::BindRepeating(&FakePluginRegistryImpl::Bind));
}

ScopedFakePluginRegistry::~ScopedFakePluginRegistry() {
  Platform::Current()->GetBrowserInterfaceBrokerProxy()->SetBinderForTesting(
      mojom::blink::PluginRegistry::Name_, {});
}

}  // namespace blink
