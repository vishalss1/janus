#include "Device.h"
#include "Queue.h"

NTSTATUS
VirtualHidCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    NTSTATUS status;

    // Initialize device attributes and register context
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext = GetDeviceContext(device);
    deviceContext->Device = device;
    deviceContext->MouseButtonState = 0;
    deviceContext->MouseLastX = 0;
    deviceContext->MouseLastY = 0;

    // Create device interface so application can find this driver instance
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_VIRTUAL_HID,
        NULL // RefString
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Initialize I/O queue
    status = VirtualHidQueueInitialize(device);
    return status;
}
