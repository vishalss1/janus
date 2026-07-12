#include "Queue.h"
#include "Device.h"
#include <hidclass.h>

// Mock HID Report Descriptor for a standard 3-button mouse
const UCHAR DefaultReportDescriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

const HID_DESCRIPTOR DefaultHidDescriptor = {
    sizeof(HID_DESCRIPTOR),             // bLength
    HID_HID_DESCRIPTOR_TYPE,            // bDescriptorType
    0x0110,                             // bcdHID (1.10)
    0x00,                               // bCountryCode
    0x01,                               // bNumDescriptors
    {                                   // DescriptorList
        HID_REPORT_DESCRIPTOR_TYPE,     // bReportType
        sizeof(DefaultReportDescriptor) // wDescriptorLength
    }
};

NTSTATUS
VirtualHidQueueInitialize(
    _In_ WDFDEVICE Device
)
{
    WDF_OBJECT_ATTRIBUTES queueAttributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(Device);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = VirtualHidEvtIoDeviceControl;

    WDF_OBJECT_ATTRIBUTES_INIT(&queueAttributes);

    status = WdfIoQueueCreate(
        Device,
        &queueConfig,
        &queueAttributes,
        &deviceContext->DefaultQueue
    );

    return status;
}

VOID
VirtualHidEvtIoDeviceControl(
    _In_ WDFQUEUE      Queue,
    _In_ WDFREQUEST    Request,
    _In_ size_t        OutputBufferLength,
    _In_ size_t        InputBufferLength,
    _In_ ULONG         IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT deviceContext = GetDeviceContext(device);
    size_t bytesTransferred = 0;
    PVOID buffer = NULL;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        // HID class driver wants to fetch the basic device structure
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(HID_DESCRIPTOR), &buffer, NULL);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(buffer, &DefaultHidDescriptor, sizeof(HID_DESCRIPTOR));
            bytesTransferred = sizeof(HID_DESCRIPTOR);
        }
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        // HID class driver wants to fetch our report descriptor structure (mapping data payload)
        if (OutputBufferLength < sizeof(DefaultReportDescriptor)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(DefaultReportDescriptor), &buffer, NULL);
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(buffer, DefaultReportDescriptor, sizeof(DefaultReportDescriptor));
            bytesTransferred = sizeof(DefaultReportDescriptor);
        }
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        // Return mock vendor/product attributes
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(HID_DEVICE_ATTRIBUTES), &buffer, NULL);
        if (NT_SUCCESS(status)) {
            PHID_DEVICE_ATTRIBUTES attr = (PHID_DEVICE_ATTRIBUTES)buffer;
            attr->Size = sizeof(HID_DEVICE_ATTRIBUTES);
            attr->VendorID = 0xFEED;  // Mock Vendor
            attr->ProductID = 0xDEAF; // Mock Product
            attr->VersionNumber = 1;
            bytesTransferred = sizeof(HID_DEVICE_ATTRIBUTES);
        }
        break;

    case IOCTL_WRITE_GESTURE_INPUT:
        // Custom IOCTL called by C++ pipeline app.
        // It provides a new physical mouse movement payload to the virtual driver.
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(GESTURE_MOUSE_REPORT), &buffer, NULL);
        if (NT_SUCCESS(status)) {
            PGESTURE_MOUSE_REPORT report = (PGESTURE_MOUSE_REPORT)buffer;
            deviceContext->MouseButtonState = report->Buttons;
            deviceContext->MouseLastX = report->X;
            deviceContext->MouseLastY = report->Y;
            
            // In a production UMDF2 driver, we would complete a pending IOCTL_HID_READ_REPORT request
            // with this mouse data so the Windows OS processes the motion.
            bytesTransferred = sizeof(GESTURE_MOUSE_REPORT);
        }
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesTransferred);
}
