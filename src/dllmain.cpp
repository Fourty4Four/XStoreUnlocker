// XStoreAPI Unlocker v2.0.1
// xgameruntime.dll proxy. rename original to XGameRuntime_o.dll

#include "proxy.h"
#include "store_hooks.h"
#include "config.h"
#include "logger.h"

#include <string>
#include <Windows.h>

static HMODULE g_module = nullptr;
static UnlockerConfig g_config;

static std::string GetDllDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(g_module, path, MAX_PATH);
    std::string dir(path);
    auto pos = dir.find_last_of("\\/");
    return (pos != std::string::npos) ? dir.substr(0, pos) : dir;
}

extern "C" __declspec(dllexport) HRESULT __cdecl QueryApiImpl(
    const GUID* providerGuid,
    const GUID* interfaceGuid,
    void** ppInterface)
{
    if (!Proxy::EnsureInitialized()) return E_FAIL;

    HRESULT hr = Proxy::GetReal_QueryApiImpl()(providerGuid, interfaceGuid, ppInterface);

    if (SUCCEEDED(hr) && ppInterface && *ppInterface) {
        if (GuidsEqual(providerGuid, &XSTORE_PROVIDER_GUID)) {
            StoreHooks::OnStoreInterfaceCreated(ppInterface);
        }
    }

    return hr;
}

extern "C" __declspec(dllexport) HRESULT __cdecl InitializeApiImpl(uint64_t a1, uint64_t a2) {
    if (!Proxy::EnsureInitialized()) return E_FAIL;
    auto fn = Proxy::GetReal_InitializeApiImpl();
    return fn ? fn(a1, a2) : E_NOTIMPL;
}

extern "C" __declspec(dllexport) HRESULT __cdecl InitializeApiImplEx(uint64_t a1, uint64_t a2, int64_t a3) {
    if (!Proxy::EnsureInitialized()) return E_FAIL;
    auto fn = Proxy::GetReal_InitializeApiImplEx();
    return fn ? fn(a1, a2, a3) : E_NOTIMPL;
}

extern "C" __declspec(dllexport) HRESULT __cdecl InitializeApiImplEx2(uint64_t a1, uint64_t a2, int64_t a3, int64_t a4) {
    if (!Proxy::EnsureInitialized()) return E_FAIL;
    auto fn = Proxy::GetReal_InitializeApiImplEx2();
    return fn ? fn(a1, a2, a3, a4) : E_NOTIMPL;
}

extern "C" __declspec(dllexport) HRESULT __cdecl UninitializeApiImpl() {
    if (!Proxy::EnsureInitialized()) return E_FAIL;
    auto fn = Proxy::GetReal_UninitializeApiImpl();
    return fn ? fn() : E_NOTIMPL;
}

extern "C" __declspec(dllexport) HRESULT __cdecl DllCanUnloadNow() {
    if (!Proxy::EnsureInitialized()) return S_FALSE;
    auto fn = Proxy::GetReal_DllCanUnloadNow();
    return fn ? fn() : S_FALSE;
}

extern "C" __declspec(dllexport) void __cdecl XErrorReport(uint64_t code, const char* message) {
    if (!Proxy::EnsureInitialized()) return;
    auto fn = Proxy::GetReal_XErrorReport();
    if (fn) fn(code, message);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
    {
        g_module = hModule;
        DisableThreadLibraryCalls(hModule);

        std::string dir = GetDllDirectory();

        Logger::Instance().Init(dir + "\\xstore_unlocker.log");
        LOG_INFO("XStoreAPI Unlocker v2.0.1");

        g_config = LoadConfig(dir + "\\xstore_unlocker.ini");
        Logger::Instance().SetEnabled(g_config.logEnabled);

        Proxy::SetOurModule(hModule);
        StoreHooks::Initialize(g_config);

        LOG_INFO("Ready. Real DLL loads on first API call.");
        break;
    }
    case DLL_PROCESS_DETACH:
        StoreHooks::Shutdown();
        Proxy::Shutdown(lpReserved != nullptr);
        break;
    }

    return TRUE;
}
