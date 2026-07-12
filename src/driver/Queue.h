#pragma once

#include "Driver.h"

// Custom IOCTL for core pipeline app to send mouse/keyboard reports to the driver
#define IOCTL_WRITE_GESTURE_INPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// Mouse report structure sent by C++ app to the driver
typedef struct _GESTURE_MOUSE_REPORT {
    UCHAR Buttons;
    CHAR  X;
    CHAR  Y;
    CHAR  Wheel;
} GESTURE_MOUSE_REPORT, *PGESTURE_MOUSE_REPORT;

NTSTATUS
VirtualHidQueueInitialize(
    _In_ WDFDEVICE Device
);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL VirtualHidEvtIoDeviceControl;
