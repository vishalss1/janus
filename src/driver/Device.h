#pragma once

#include "Driver.h"

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE DefaultQueue;
    
    // HID Report structures
    UCHAR MouseButtonState;
    LONG MouseLastX;
    LONG MouseLastY;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

NTSTATUS
VirtualHidCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
);
