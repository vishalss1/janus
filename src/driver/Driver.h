#pragma once

#include <windows.h>
#include <wdf.h>
#include <initguid.h>

// {A0B2C4E6-80C0-499D-B1DF-356C19D8E6F0}
DEFINE_GUID(GUID_DEVINTERFACE_VIRTUAL_HID, 
0xa0b2c4e6, 0x80c0, 0x499d, 0xb1, 0xdf, 0x35, 0x6c, 0x19, 0xd8, 0xe6, 0xf0);

// Driver tracking context
typedef struct _DRIVER_CONTEXT {
    ULONG SampleField;
} DRIVER_CONTEXT, *PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DRIVER_CONTEXT, GetDriverContext)

// Prototypes
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD VirtualHidEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP VirtualHidEvtDriverContextCleanup;
