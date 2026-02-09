// XStore type definitions. Reconstructed from XGameRuntime.dll via IDA.
// 85-entry COM vtable dispatches all XStore API calls.

#pragma once

#include <cstdint>
#include <Windows.h>

constexpr size_t STORE_VTABLE_SIZE = 85;

// Generic vtable entry. All XStore methods use __fastcall (x64 default).
typedef int64_t(__fastcall* VtableEntry_t)(void*, ...);

// Vtable index map. Derived from storeimpl.cpp debug strings in the binary.
namespace StoreVtable {
    constexpr int QueryInterface                        = 0;
    constexpr int AddRef                                = 1;
    constexpr int Release                               = 2;
    constexpr int CreateContext                          = 3;
    constexpr int CloseContext                           = 4;
    constexpr int QueryAssociatedProductsAsync           = 5;
    constexpr int QueryAssociatedProductsResult          = 6;
    constexpr int QueryProductsAsync                     = 7;
    constexpr int QueryProductsResult                    = 8;
    constexpr int QueryEntitledProductsAsync             = 9;
    constexpr int QueryEntitledProductsResult            = 10;
    constexpr int QueryProductForCurrentGameAsync        = 11;
    constexpr int QueryProductForCurrentGameResult       = 12;
    constexpr int QueryProductForPackageAsync            = 13;
    constexpr int QueryProductForPackageResult           = 14;
    constexpr int ProductsQueryGetProducts               = 15;
    constexpr int ProductsQueryHasMorePages              = 16;
    constexpr int ProductsQueryNextPageAsync             = 17;
    constexpr int ProductsQueryNextPageResult            = 18;
    constexpr int ProductsQueryClose                     = 19;
    constexpr int AcquireLicenseForPackageAsync          = 20;
    constexpr int AcquireLicenseForPackageResult         = 21;
    constexpr int LicenseIsValid                         = 22;
    constexpr int CloseLicenseHandle                     = 23;
    constexpr int CanAcquireLicenseForStoreIdAsync       = 24;
    constexpr int CanAcquireLicenseForStoreIdResult      = 25;
    constexpr int CanAcquireLicenseForPackageAsync       = 26;
    constexpr int CanAcquireLicenseForPackageResult      = 27;
    constexpr int QueryGameLicenseAsync                  = 28;
    constexpr int QueryGameLicenseResult                 = 29;
    constexpr int QueryAddOnLicensesAsync                = 30;
    constexpr int QueryAddOnLicensesResult               = 31;
    constexpr int QueryAddOnLicensesGetResult            = 32;
    constexpr int QueryConsumableBalanceAsync             = 33;
    constexpr int QueryConsumableBalanceResult            = 34;
    constexpr int ReportConsumableFulfillmentAsync        = 35;
    constexpr int ReportConsumableFulfillmentResult       = 36;
    constexpr int GetUserCollectionsIdAsync               = 37;
    constexpr int GetUserCollectionsIdResult              = 38;
    constexpr int GetUserCollectionsIdSize                = 39;
    constexpr int GetUserPurchaseIdAsync                  = 40;
    constexpr int GetUserPurchaseIdResult                 = 41;
    constexpr int GetUserPurchaseIdSize                   = 42;
    constexpr int QueryLicenseTokenAsync                  = 43;
    constexpr int QueryLicenseTokenResult                 = 44;
    constexpr int QueryLicenseTokenSize                   = 45;
    constexpr int SendRequestAsync                        = 46;
    constexpr int SendRequestResult                       = 47;
    constexpr int SendRequestResultSize                   = 48;
    constexpr int ShowPurchaseUIAsync                     = 49;
    constexpr int ShowPurchaseUIResult                    = 50;
    constexpr int ShowRateAndReviewUIAsync                = 51;
    constexpr int ShowRateAndReviewUIResult               = 52;
    constexpr int ShowRedeemTokenUIAsync                  = 53;
    constexpr int ShowRedeemTokenUIResult                 = 54;
    constexpr int QueryGameAndDlcPackageUpdatesAsync      = 55;
    constexpr int QueryGameAndDlcPackageUpdatesResult     = 56;
    constexpr int QueryGameAndDlcPackageUpdatesCount      = 57;
    constexpr int DownloadPackageUpdatesAsync             = 58;
    constexpr int DownloadPackageUpdatesResult            = 59;
    constexpr int DownloadAndInstallPackageUpdatesAsync   = 60;
    constexpr int DownloadAndInstallPackageUpdatesResult  = 61;
    constexpr int DownloadAndInstallPackagesAsync         = 62;
    constexpr int DownloadAndInstallPackagesResult        = 63;
    constexpr int DownloadAndInstallPackagesCount         = 64;
    constexpr int RegisterPackageInstallMonitor           = 65;
    constexpr int RegisterGameLicenseChanged              = 66;
    constexpr int RegisterPackageLicenseLost              = 67;
    constexpr int UnregisterEvent                         = 68;
    constexpr int RegisterPackageInstallProgress          = 69;
    constexpr int IsAvailabilityValid                     = 70;
    constexpr int AcquireLicenseForDurablesAsync          = 71;
    constexpr int AcquireLicenseForDurablesResult         = 72;
    constexpr int ShowAssociatedProductsUIAsync           = 73;
    constexpr int ShowAssociatedProductsUIResult          = 74;
    constexpr int ShowProductPageUIAsync                  = 75;
    constexpr int ShowProductPageUIResult                 = 76;
    constexpr int QueryAssociatedProductsForStoreIdAsync  = 77;
    constexpr int QueryAssociatedProductsForStoreIdResult = 78;
    constexpr int QueryPackageUpdatesAsync                = 79;
    constexpr int QueryPackageUpdatesResult               = 80;
    constexpr int QueryPackageUpdatesCount                = 81;
    constexpr int ShowGiftingUIAsync                      = 82;
    constexpr int ShowGiftingUIResult                     = 83;
    constexpr int UnregisterStorePackageUpdate            = 84;
}

// Export function signatures. Derived from IDA decompilation of XGameRuntime.dll.
typedef HRESULT(__cdecl* QueryApiImpl_t)(const GUID*, const GUID*, void**);
typedef HRESULT(__cdecl* InitializeApiImpl_t)(uint64_t, uint64_t);
typedef HRESULT(__cdecl* InitializeApiImplEx_t)(uint64_t, uint64_t, int64_t);
typedef HRESULT(__cdecl* InitializeApiImplEx2_t)(uint64_t, uint64_t, int64_t, int64_t);
typedef HRESULT(__cdecl* UninitializeApiImpl_t)(void);
typedef HRESULT(__cdecl* DllCanUnloadNow_t)(void);
typedef void(__cdecl* XErrorReport_t)(uint64_t, const char*);
