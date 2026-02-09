// XStore vtable hooks. By ZephKek.
// Offsets from IDA analysis of xgameruntime.dll storeimpl.

#include "store_hooks.h"
#include "proxy.h"
#include "logger.h"
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <string>
#include <vector>

static UnlockerConfig g_cfg;
static VtableEntry_t g_orig[STORE_VTABLE_SIZE] = {};
static std::mutex g_mtx;

constexpr size_t STRIDE              = 208;  // product struct size
constexpr size_t OFF_ID              = 0;    // char* storeId
constexpr size_t OFF_TITLE           = 8;    // char* title
constexpr size_t OFF_OWNED           = 145;  // byte isInUserCollection
constexpr size_t OFF_VALID           = 4;    // license handle isValid
constexpr size_t OFF_ARR_START       = 24;   // query handle array begin
constexpr size_t OFF_ARR_END         = 32;   // query handle array end
constexpr size_t OFF_CAN_ACQUIRE_STATUS  = 64;  // {storeId[64]; status}
constexpr size_t OFF_GAME_LICENSE_ACTIVE = 64;  // {skuStoreId[64]; isActive}
constexpr uint32_t LICENSE_STATUS_ACTIVE = 0;

typedef int64_t(__fastcall* Fn2)(void*, void*);
typedef int64_t(__fastcall* Fn3)(void*, void*, void*);
typedef int64_t(__fastcall* Fn4)(void*, void*, void*, void*);
typedef uint8_t(__fastcall* ProductCb_t)(void* product, void* context);

// Persistent storage for injected product strings so pointers stay valid
static std::vector<std::string> g_injectedIds;
static bool g_injectedOnce = false;

// vt[15] GetProducts - patch real products, inject fake ones for [DLCs] IDs
static int64_t __fastcall Hook_GetProducts(void* self, void* qh, void* ctx, void* cb) {
    std::unordered_set<std::string> seen;
    int total = 0, patched = 0;

    if (qh) {
        uint8_t* base = *(uint8_t**)((uint8_t*)qh + OFF_ARR_START);
        uint8_t* end  = *(uint8_t**)((uint8_t*)qh + OFF_ARR_END);

        for (uint8_t* p = base; p < end; p += STRIDE) {
            total++;
            const char* id    = *(const char**)(p + OFF_ID);
            const char* title = *(const char**)(p + OFF_TITLE);

            if (id) seen.insert(id);

            if (id && g_cfg.blacklist.count(id)) {
                LOG_INFO("[SKIP] '%s' (%s) blacklisted", title ? title : "", id);
                continue;
            }

            bool shouldOwn = g_cfg.unlockAll || (id && g_cfg.dlcs.count(id));
            if (shouldOwn && !p[OFF_OWNED]) {
                p[OFF_OWNED] = 1;
                patched++;
                LOG_INFO("[PATCH] '%s' (%s) set to owned", title ? title : "", id ? id : "");
            }
        }

        LOG_INFO("GetProducts: %d total, %d patched", total, patched);
    }

    // Call original - delivers real products to game via callback
    int64_t hr = ((Fn4)g_orig[15])(self, qh, ctx, cb);

    // Inject fake products for [DLCs] IDs not in real results
    if (hr == 0 && cb && !g_cfg.dlcs.empty()) {
        auto callback = (ProductCb_t)cb;
        int injected = 0;

        // Build persistent ID storage on first call
        if (!g_injectedOnce) {
            for (const auto& id : g_cfg.dlcs) {
                g_injectedIds.push_back(id);
            }
            g_injectedOnce = true;
        }

        for (const auto& id : g_injectedIds) {
            if (seen.count(id)) continue;
            if (g_cfg.blacklist.count(id)) continue;

            uint8_t fake[STRIDE] = {};
            *(const char**)(fake + OFF_ID)    = id.c_str();
            *(const char**)(fake + OFF_TITLE) = id.c_str();
            fake[OFF_OWNED] = 1;

            callback(fake, ctx);
            injected++;
            LOG_INFO("[INJECT] %s injected as owned", id.c_str());
        }

        if (injected > 0) {
            LOG_INFO("Injected %d unlisted products", injected);
        }
    }

    return hr;
}

// vt[22] LicenseIsValid - always true
static int64_t __fastcall Hook_LicenseIsValid(void* self, void* handle) {
    ((Fn2)g_orig[22])(self, handle);
    return 1;
}

// vt[21] PackageLicenseResult - force isValid
static int64_t __fastcall Hook_PackageLicenseResult(void* self, void* async, void** out) {
    int64_t hr = ((Fn3)g_orig[21])(self, async, out);
    if (hr == 0 && out && *out) {
        ((uint8_t*)*out)[OFF_VALID] = 1;
        LOG_INFO("PackageLicenseResult: handle=%p forced valid", *out);
    }
    return hr;
}

// vt[72] DurablesLicenseResult - force isValid
static int64_t __fastcall Hook_DurablesLicenseResult(void* self, void* async, void** out) {
    int64_t hr = ((Fn3)g_orig[72])(self, async, out);
    if (hr == 0 && out && *out) {
        ((uint8_t*)*out)[OFF_VALID] = 1;
        LOG_INFO("DurablesLicenseResult: handle=%p forced valid", *out);
    }
    return hr;
}

// vt[25] CanAcquireLicenseForStoreIdResult - force Active
static int64_t __fastcall Hook_CanAcquireForStoreIdResult(void* self, void* async, void* out) {
    int64_t hr = ((Fn3)g_orig[25])(self, async, out);
    if (hr == 0 && out) {
        *(uint32_t*)((uint8_t*)out + OFF_CAN_ACQUIRE_STATUS) = LICENSE_STATUS_ACTIVE;
        LOG_INFO("CanAcquireLicenseForStoreId: forced Active");
    }
    return hr;
}

// vt[27] CanAcquireLicenseForPackageResult - force Active
static int64_t __fastcall Hook_CanAcquireForPackageResult(void* self, void* async, void* out) {
    int64_t hr = ((Fn3)g_orig[27])(self, async, out);
    if (hr == 0 && out) {
        *(uint32_t*)((uint8_t*)out + OFF_CAN_ACQUIRE_STATUS) = LICENSE_STATUS_ACTIVE;
        LOG_INFO("CanAcquireLicenseForPackage: forced Active");
    }
    return hr;
}

// vt[29] GameLicenseResult - force isActive
static int64_t __fastcall Hook_GameLicenseResult(void* self, void* async, void* out) {
    int64_t hr = ((Fn3)g_orig[29])(self, async, out);
    if (hr == 0 && out) {
        ((uint8_t*)out)[OFF_GAME_LICENSE_ACTIVE] = 1;
        LOG_INFO("QueryGameLicenseResult: forced isActive=true");
    }
    return hr;
}

// vt[20] PackageLicenseAsync - log only
static int64_t __fastcall Hook_PackageLicenseAsync(void* self, void* ctx, void* pkg, void* async) {
    LOG_INFO("AcquireLicenseForPackage: '%s'", pkg ? (const char*)pkg : "");
    return ((Fn4)g_orig[20])(self, ctx, pkg, async);
}

// vt[71] DurablesLicenseAsync - log only
static int64_t __fastcall Hook_DurablesLicenseAsync(void* self, void* ctx, void* id, void* async) {
    LOG_INFO("AcquireLicenseForDurables: '%s'", id ? (const char*)id : "");
    return ((Fn4)g_orig[71])(self, ctx, id, async);
}

void StoreHooks::Initialize(const UnlockerConfig& cfg) {
    g_cfg = cfg;
    LOG_INFO("Hooks ready. unlock_all=%d", cfg.unlockAll);
}

void StoreHooks::OnStoreInterfaceCreated(void** ppInterface) {
    if (!ppInterface || !*ppInterface) return;

    std::lock_guard<std::mutex> lock(g_mtx);

    static bool s_done = false;
    if (s_done) return;
    s_done = true;

    void* obj = *ppInterface;
    VtableEntry_t* vt = *(VtableEntry_t**)obj;

    LOG_INFO("Store vtable at %p. Installing hooks.", vt);

    memcpy(g_orig, vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE);

    DWORD old;
    if (!VirtualProtect(vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, PAGE_READWRITE, &old)) {
        LOG_ERROR("VirtualProtect failed (err=%lu)", GetLastError());
        return;
    }

    vt[15] = (VtableEntry_t)&Hook_GetProducts;
    vt[20] = (VtableEntry_t)&Hook_PackageLicenseAsync;
    vt[21] = (VtableEntry_t)&Hook_PackageLicenseResult;
    vt[22] = (VtableEntry_t)&Hook_LicenseIsValid;
    vt[25] = (VtableEntry_t)&Hook_CanAcquireForStoreIdResult;
    vt[27] = (VtableEntry_t)&Hook_CanAcquireForPackageResult;
    vt[29] = (VtableEntry_t)&Hook_GameLicenseResult;
    vt[71] = (VtableEntry_t)&Hook_DurablesLicenseAsync;
    vt[72] = (VtableEntry_t)&Hook_DurablesLicenseResult;

    VirtualProtect(vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, old, &old);

    LOG_INFO("9 hooks installed: vtable[15, 20, 21, 22, 25, 27, 29, 71, 72]");
}
