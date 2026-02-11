// vtable hooks for XStore COM interface

#pragma once

#include "xstore_types.h"
#include "config.h"

namespace StoreHooks {
    void Initialize(const UnlockerConfig& cfg);
    void OnStoreInterfaceCreated(void** ppInterface);
    void Shutdown();
}
