// Proxy loader. Forwards all 7 exports to the real XGameRuntime_o.dll.

#pragma once

#include "xstore_types.h"
#include <Windows.h>

namespace Proxy {
    void SetOurModule(HMODULE m);
    bool EnsureInitialized();
    void Shutdown();

    QueryApiImpl_t          GetReal_QueryApiImpl();
    InitializeApiImpl_t     GetReal_InitializeApiImpl();
    InitializeApiImplEx_t   GetReal_InitializeApiImplEx();
    InitializeApiImplEx2_t  GetReal_InitializeApiImplEx2();
    UninitializeApiImpl_t   GetReal_UninitializeApiImpl();
    DllCanUnloadNow_t       GetReal_DllCanUnloadNow();
    XErrorReport_t          GetReal_XErrorReport();
}

// Store provider GUID: {0DD112AC-7C24-448C-B92B-3960FB5BD30C}
static const GUID XSTORE_PROVIDER_GUID = {
    0x0DD112AC, 0x7C24, 0x448C,
    { 0xB9, 0x2B, 0x39, 0x60, 0xFB, 0x5B, 0xD3, 0x0C }
};

inline bool GuidsEqual(const GUID* a, const GUID* b) {
    return memcmp(a, b, sizeof(GUID)) == 0;
}
