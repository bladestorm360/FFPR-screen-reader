#pragma once
// The 47 named exports of System32\hid.dll (no ordinal-only or data exports), checked with
// pefile on 2026-10-05. tools/import_check.py compares the built DLL against the real one at
// every build, so a Windows update that changes the list fails the build, not the game.
#define HID_EXPORTS(X) \
    X(HidD_FlushQueue) \
    X(HidD_FreePreparsedData) \
    X(HidD_GetAttributes) \
    X(HidD_GetConfiguration) \
    X(HidD_GetFeature) \
    X(HidD_GetHidGuid) \
    X(HidD_GetIndexedString) \
    X(HidD_GetInputReport) \
    X(HidD_GetManufacturerString) \
    X(HidD_GetMsGenreDescriptor) \
    X(HidD_GetNumInputBuffers) \
    X(HidD_GetPhysicalDescriptor) \
    X(HidD_GetPreparsedData) \
    X(HidD_GetProductString) \
    X(HidD_GetSerialNumberString) \
    X(HidD_Hello) \
    X(HidD_SetConfiguration) \
    X(HidD_SetFeature) \
    X(HidD_SetNumInputBuffers) \
    X(HidD_SetOutputReport) \
    X(HidP_GetButtonArray) \
    X(HidP_GetButtonCaps) \
    X(HidP_GetCaps) \
    X(HidP_GetData) \
    X(HidP_GetExtendedAttributes) \
    X(HidP_GetLinkCollectionNodes) \
    X(HidP_GetScaledUsageValue) \
    X(HidP_GetSpecificButtonCaps) \
    X(HidP_GetSpecificValueCaps) \
    X(HidP_GetUsageValue) \
    X(HidP_GetUsageValueArray) \
    X(HidP_GetUsages) \
    X(HidP_GetUsagesEx) \
    X(HidP_GetValueCaps) \
    X(HidP_GetVersionInternal) \
    X(HidP_InitializeReportForID) \
    X(HidP_MaxDataListLength) \
    X(HidP_MaxUsageListLength) \
    X(HidP_SetButtonArray) \
    X(HidP_SetData) \
    X(HidP_SetScaledUsageValue) \
    X(HidP_SetUsageValue) \
    X(HidP_SetUsageValueArray) \
    X(HidP_SetUsages) \
    X(HidP_TranslateUsagesToI8042ScanCodes) \
    X(HidP_UnsetUsages) \
    X(HidP_UsageListDifference)

constexpr int kHidExportCount = 47;
