#include "Driver.h"
#include "Device.h"

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, VirtualHidEvtDeviceAdd);
    config.EvtDriverUnload = NULL; // UMDF drivers don't strictly require EvtDriverUnload unless allocating non-WDF resources.

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DRIVER_CONTEXT);
    attributes.EvtCleanupCallback = VirtualHidEvtDriverContextCleanup;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        // OutputDebugString or WPP tracing can be set here.
    }

    return status;
}

NTSTATUS
VirtualHidEvtDeviceAdd(
    _In_ WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER(Driver);

    status = VirtualHidCreateDevice(DeviceInit);
    return status;
}

VOID
VirtualHidEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);
    // Driver-wide cleanup goes here.
}
