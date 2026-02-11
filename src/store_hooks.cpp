// XStore vtable hooks for xgameruntime.dll
// offsets verified in IDA (md5 e0c40ce5efb4a3cd8e7b0058e35399a9)

#include "store_hooks.h"
#include "proxy.h"
#include "logger.h"
#include <atomic>
#include <cstring>
#include <unordered_set>
#include <string>
#include <vector>
#include <memory>

static UnlockerConfig g_cfg;
static VtableEntry_t  g_orig[STORE_VTABLE_SIZE] = {};
static SRWLOCK        g_hookLock  = SRWLOCK_INIT;
static SRWLOCK        g_fakesLock = SRWLOCK_INIT;
static VtableEntry_t* g_hookedVtable = nullptr;
static std::atomic<bool> g_shutdown{false};

// XStoreProduct (208 bytes, sub_1800DABC0)
constexpr size_t STRIDE        = 208;
constexpr size_t OFF_ID        = 0;     // char*
constexpr size_t OFF_TITLE     = 8;     // char*
constexpr size_t OFF_DESC      = 16;    // char*
constexpr size_t OFF_LANG      = 24;    // char*
constexpr size_t OFF_OFFER     = 32;    // char*
constexpr size_t OFF_LINK      = 40;    // char*
constexpr size_t OFF_KIND      = 48;    // uint32 XStoreProductKind
constexpr size_t OFF_CURRENCY  = 72;    // char* (XStorePrice at +56)
constexpr size_t OFF_FMT_BASE  = 80;    // char[16] inline
constexpr size_t OFF_FMT_PRICE = 96;    // char[16] inline
constexpr size_t OFF_FMT_REC   = 112;   // char[16] inline
constexpr size_t OFF_HAS_DL    = 144;   // bool hasDigitalDownload
constexpr size_t OFF_OWNED     = 145;   // bool isInUserCollection

constexpr size_t OFF_LICENSE_VALID      = 4;   // byte in license handle (sub_1800DEAB0)
constexpr size_t OFF_ARR_START          = 24;  // query handle: array base
constexpr size_t OFF_ARR_END            = 32;  // query handle: array end
constexpr size_t OFF_CAN_ACQUIRE_STATUS = 8;   // uint32 in XStoreCanAcquireLicenseResult
constexpr size_t OFF_GAME_LICENSE_ACTIVE = 18;  // bool in XStoreGameLicense
constexpr size_t OFF_ADDON_IS_ACTIVE    = 82;   // bool in XStoreAddonLicense

typedef int64_t(__fastcall* Fn2)(void*, void*);
typedef int64_t(__fastcall* Fn3)(void*, void*, void*);
typedef int64_t(__fastcall* Fn4)(void*, void*, void*, void*);
typedef uint8_t(__fastcall* ProductCb_t)(void*, void*);

// all string pointers point to static storage or std::string members
// so they stay valid for the process lifetime
struct FakeProduct {
    std::string storeId;
    std::unique_ptr<uint8_t[]> data;

    FakeProduct(const std::string& id)
        : storeId(id)
        , data(std::make_unique<uint8_t[]>(STRIDE))
    {
        memset(data.get(), 0, STRIDE);
        uint8_t* p = data.get();

        *(const char**)(p + OFF_ID)       = storeId.c_str();
        *(const char**)(p + OFF_TITLE)    = storeId.c_str();
        *(const char**)(p + OFF_DESC)     = "";
        *(const char**)(p + OFF_LANG)     = "en-US";
        *(const char**)(p + OFF_OFFER)    = "";
        *(const char**)(p + OFF_LINK)     = "";
        *(uint32_t*)(p + OFF_KIND)        = PRODUCT_KIND_DURABLE;
        *(const char**)(p + OFF_CURRENCY) = "USD";
        strncpy((char*)(p + OFF_FMT_BASE),  "$0.00", PRICE_MAX_SIZE);
        strncpy((char*)(p + OFF_FMT_PRICE), "$0.00", PRICE_MAX_SIZE);
        strncpy((char*)(p + OFF_FMT_REC),   "$0.00", PRICE_MAX_SIZE);
        p[OFF_HAS_DL] = 1;
        p[OFF_OWNED] = 1;
    }

    FakeProduct(const FakeProduct&) = delete;
    FakeProduct& operator=(const FakeProduct&) = delete;
};

static std::vector<std::unique_ptr<FakeProduct>> g_fakeProducts;
static std::atomic<bool> g_fakesBuilt{false};

// global dedup: real IDs + injected fake IDs across all queries
static std::unordered_set<std::string> g_seenIds;
static SRWLOCK g_seenLock = SRWLOCK_INIT;

static void BuildFakeProducts() {
    if (g_fakesBuilt) return;
    AcquireSRWLockExclusive(&g_fakesLock);
    if (!g_fakesBuilt) {
        for (const auto& id : g_cfg.dlcs) {
            if (g_cfg.blacklist.count(id)) continue;
            g_fakeProducts.push_back(std::make_unique<FakeProduct>(id));
        }
        g_fakesBuilt = true;
        LOG_INFO("built %zu fake products", g_fakeProducts.size());
    }
    ReleaseSRWLockExclusive(&g_fakesLock);
}

// vt[15]: patch owned flag on real products, inject fakes for missing DLC IDs
static int64_t __fastcall Hook_GetProducts(void* self, void* qh, void* ctx, void* cb) {
    if (g_shutdown) return ((Fn4)g_orig[StoreVtable::ProductsQueryGetProducts])(self, qh, ctx, cb);
    std::unordered_set<std::string> localSeen;
    int total = 0, patched = 0;

    if (qh) {
        auto* base = *(uint8_t**)((uint8_t*)qh + OFF_ARR_START);
        auto* end  = *(uint8_t**)((uint8_t*)qh + OFF_ARR_END);

        for (uint8_t* p = base; base && p < end; p += STRIDE) {
            total++;
            const char* id    = *(const char**)(p + OFF_ID);
            const char* title = *(const char**)(p + OFF_TITLE);
            if (id) localSeen.insert(id);
            if (id && g_cfg.blacklist.count(id)) continue;

            if ((g_cfg.unlockAll || (id && g_cfg.dlcs.count(id))) && !p[OFF_OWNED]) {
                p[OFF_OWNED] = 1;
                patched++;
                LOG_INFO("[PATCH] '%s' (%s)", title ? title : "", id ? id : "");
            }
        }
        LOG_INFO("GetProducts: %d real, %d patched", total, patched);
    }

    int64_t hr = ((Fn4)g_orig[StoreVtable::ProductsQueryGetProducts])(self, qh, ctx, cb);

    if (hr == 0 && cb && !g_cfg.dlcs.empty()) {
        BuildFakeProducts();

        // collect fakes to inject under lock, call game callback after releasing
        std::vector<uint8_t*> pending;

        AcquireSRWLockExclusive(&g_seenLock);
        for (const auto& id : localSeen)
            g_seenIds.insert(id);
        AcquireSRWLockShared(&g_fakesLock);
        for (const auto& fake : g_fakeProducts) {
            if (g_seenIds.count(fake->storeId)) continue;
            g_seenIds.insert(fake->storeId);
            pending.push_back(fake->data.get());
        }
        ReleaseSRWLockShared(&g_fakesLock);
        ReleaseSRWLockExclusive(&g_seenLock);

        auto callback = (ProductCb_t)cb;
        for (auto* p : pending)
            if (!callback(p, ctx)) break;

        if (!pending.empty())
            LOG_INFO("injected %zu fake products", pending.size());
    }

    return hr;
}

// vt[22]: force license valid
static int64_t __fastcall Hook_LicenseIsValid(void* self, void* handle) {
    if (g_shutdown) return ((Fn2)g_orig[StoreVtable::LicenseIsValid])(self, handle);
    ((Fn2)g_orig[StoreVtable::LicenseIsValid])(self, handle);
    return 1;
}

// vt[21]: force package license handle valid
static int64_t __fastcall Hook_PackageLicenseResult(void* self, void* async, void** out) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::AcquireLicenseForPackageResult])(self, async, out);
    int64_t hr = ((Fn3)g_orig[StoreVtable::AcquireLicenseForPackageResult])(self, async, out);
    if (hr == 0 && out && *out) {
        ((uint8_t*)*out)[OFF_LICENSE_VALID] = 1;
        LOG_INFO("PackageLicenseResult: forced valid");
    }
    return hr;
}

// vt[72]: force durables license handle valid
static int64_t __fastcall Hook_DurablesLicenseResult(void* self, void* async, void** out) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::AcquireLicenseForDurablesResult])(self, async, out);
    int64_t hr = ((Fn3)g_orig[StoreVtable::AcquireLicenseForDurablesResult])(self, async, out);
    if (hr == 0 && out && *out) {
        ((uint8_t*)*out)[OFF_LICENSE_VALID] = 1;
        LOG_INFO("DurablesLicenseResult: forced valid");
    }
    return hr;
}

// vt[25]: force can-acquire status active
static int64_t __fastcall Hook_CanAcquireStoreIdResult(void* self, void* async, void* out) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::CanAcquireLicenseForStoreIdResult])(self, async, out);
    int64_t hr = ((Fn3)g_orig[StoreVtable::CanAcquireLicenseForStoreIdResult])(self, async, out);
    if (hr == 0 && out)
        *(uint32_t*)((uint8_t*)out + OFF_CAN_ACQUIRE_STATUS) = 0;
    return hr;
}

// vt[27]: force can-acquire status active
static int64_t __fastcall Hook_CanAcquirePackageResult(void* self, void* async, void* out) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::CanAcquireLicenseForPackageResult])(self, async, out);
    int64_t hr = ((Fn3)g_orig[StoreVtable::CanAcquireLicenseForPackageResult])(self, async, out);
    if (hr == 0 && out)
        *(uint32_t*)((uint8_t*)out + OFF_CAN_ACQUIRE_STATUS) = 0;
    return hr;
}

// vt[29]: force game license active, clear trial
static int64_t __fastcall Hook_GameLicenseResult(void* self, void* async, void* out) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::QueryGameLicenseResult])(self, async, out);
    int64_t hr = ((Fn3)g_orig[StoreVtable::QueryGameLicenseResult])(self, async, out);
    if (hr == 0 && out) {
        auto* p = (uint8_t*)out;
        p[OFF_GAME_LICENSE_ACTIVE]     = 1; // isActive
        p[OFF_GAME_LICENSE_ACTIVE + 1] = 1; // isTrialOwnedByThisUser
        p[OFF_GAME_LICENSE_ACTIVE + 2] = 0; // isDiscLicense
        p[OFF_GAME_LICENSE_ACTIVE + 3] = 0; // isTrial
        LOG_INFO("GameLicense: forced active");
    }
    return hr;
}

// vt[31]: log addon count
static int64_t __fastcall Hook_AddOnLicensesResult(void* self, void* async, uint32_t* count) {
    if (g_shutdown) return ((Fn3)g_orig[StoreVtable::QueryAddOnLicensesResult])(self, async, count);
    int64_t hr = ((Fn3)g_orig[StoreVtable::QueryAddOnLicensesResult])(self, async, count);
    if (hr == 0 && count)
        LOG_INFO("AddOnLicenses: %u", *count);
    return hr;
}

// vt[32]: force addon license active
static int64_t __fastcall Hook_AddOnLicensesGetResult(void* self, void* async, uint32_t idx, void* out) {
    if (g_shutdown) return ((Fn4)g_orig[StoreVtable::QueryAddOnLicensesGetResult])(self, async, (void*)(uintptr_t)idx, out);
    int64_t hr = ((Fn4)g_orig[StoreVtable::QueryAddOnLicensesGetResult])(self, async, (void*)(uintptr_t)idx, out);
    if (hr == 0 && out) {
        auto* p = (uint8_t*)out;
        // skuStoreId at +0 format "productId/skuId"
        std::string compound((const char*)p);
        auto slash = compound.find('/');
        std::string productId = (slash != std::string::npos) ? compound.substr(0, slash) : compound;
        if (!productId.empty() && g_cfg.blacklist.count(productId)) {
            LOG_INFO("[SKIP] Addon[%u] '%s'", idx, compound.c_str());
        } else if (g_cfg.unlockAll || (!productId.empty() && g_cfg.dlcs.count(productId))) {
            p[OFF_ADDON_IS_ACTIVE] = 1;
            LOG_INFO("[PATCH] Addon[%u] '%s'", idx, compound.c_str());
        }
    }
    return hr;
}

// vt[20]: log package license request
static int64_t __fastcall Hook_PackageLicenseAsync(void* self, void* ctx, void* pkg, void* async) {
    if (g_shutdown) return ((Fn4)g_orig[StoreVtable::AcquireLicenseForPackageAsync])(self, ctx, pkg, async);
    LOG_INFO("AcquireLicenseForPackage: '%s'", pkg ? (const char*)pkg : "");
    return ((Fn4)g_orig[StoreVtable::AcquireLicenseForPackageAsync])(self, ctx, pkg, async);
}

// vt[71]: log durables license request
static int64_t __fastcall Hook_DurablesLicenseAsync(void* self, void* ctx, void* id, void* async) {
    if (g_shutdown) return ((Fn4)g_orig[StoreVtable::AcquireLicenseForDurablesAsync])(self, ctx, id, async);
    LOG_INFO("AcquireLicenseForDurables: '%s'", id ? (const char*)id : "");
    return ((Fn4)g_orig[StoreVtable::AcquireLicenseForDurablesAsync])(self, ctx, id, async);
}

void StoreHooks::Initialize(const UnlockerConfig& cfg) {
    g_cfg = cfg;
    LOG_INFO("StoreHooks: unlock_all=%d, %zu dlcs, %zu blacklisted",
             cfg.unlockAll, cfg.dlcs.size(), cfg.blacklist.size());
}

static void InstallHooks(VtableEntry_t* vt) {
    memcpy(g_orig, vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE);

    DWORD old;
    if (!VirtualProtect(vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, PAGE_READWRITE, &old)) {
        LOG_ERROR("VirtualProtect failed: %lu", GetLastError());
        return;
    }

    vt[StoreVtable::ProductsQueryGetProducts]           = (VtableEntry_t)&Hook_GetProducts;
    vt[StoreVtable::AcquireLicenseForPackageAsync]      = (VtableEntry_t)&Hook_PackageLicenseAsync;
    vt[StoreVtable::AcquireLicenseForPackageResult]     = (VtableEntry_t)&Hook_PackageLicenseResult;
    vt[StoreVtable::LicenseIsValid]                     = (VtableEntry_t)&Hook_LicenseIsValid;
    vt[StoreVtable::AcquireLicenseForDurablesAsync]     = (VtableEntry_t)&Hook_DurablesLicenseAsync;
    vt[StoreVtable::AcquireLicenseForDurablesResult]    = (VtableEntry_t)&Hook_DurablesLicenseResult;
    vt[StoreVtable::CanAcquireLicenseForStoreIdResult]  = (VtableEntry_t)&Hook_CanAcquireStoreIdResult;
    vt[StoreVtable::CanAcquireLicenseForPackageResult]  = (VtableEntry_t)&Hook_CanAcquirePackageResult;
    vt[StoreVtable::QueryGameLicenseResult]             = (VtableEntry_t)&Hook_GameLicenseResult;
    vt[StoreVtable::QueryAddOnLicensesResult]           = (VtableEntry_t)&Hook_AddOnLicensesResult;
    vt[StoreVtable::QueryAddOnLicensesGetResult]        = (VtableEntry_t)&Hook_AddOnLicensesGetResult;

    VirtualProtect(vt, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, old, &old);
    g_hookedVtable = vt;
    LOG_INFO("hooks installed on vtable %p", vt);
}

void StoreHooks::Shutdown() {
    g_shutdown = true;
    AcquireSRWLockExclusive(&g_hookLock);
    if (g_hookedVtable) {
        DWORD old;
        if (VirtualProtect(g_hookedVtable, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, PAGE_READWRITE, &old)) {
            memcpy(g_hookedVtable, g_orig, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE);
            VirtualProtect(g_hookedVtable, sizeof(VtableEntry_t) * STORE_VTABLE_SIZE, old, &old);
        }
        g_hookedVtable = nullptr;
    }
    ReleaseSRWLockExclusive(&g_hookLock);
}

void StoreHooks::OnStoreInterfaceCreated(void** ppInterface) {
    if (!ppInterface || !*ppInterface) return;

    AcquireSRWLockExclusive(&g_hookLock);
    auto* vt = *(VtableEntry_t**)*ppInterface;
    if (vt != g_hookedVtable) {
        if (g_hookedVtable)
            LOG_INFO("vtable changed %p -> %p, re-hooking", g_hookedVtable, vt);
        InstallHooks(vt);
    }
    ReleaseSRWLockExclusive(&g_hookLock);
}
