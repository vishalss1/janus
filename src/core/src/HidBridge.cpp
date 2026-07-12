#include "HidBridge.h"
#include <iostream>
#include <initguid.h>

namespace gesture {

// Device interface GUID (matches WDK driver project definition)
// {A0B2C4E6-80C0-499D-B1DF-356C19D8E6F0}
DEFINE_GUID(GUID_DEVINTERFACE_VIRTUAL_HID, 
0xa0b2c4e6, 0x80c0, 0x499d, 0xb1, 0xdf, 0x35, 0x6c, 0x19, 0xd8, 0xe6, 0xf0);

HidBridge::HidBridge() 
    : m_hDevice(INVALID_HANDLE_VALUE)
    , m_lastX(0.5f)
    , m_lastY(0.5f)
    , m_firstUpdate(true)
{}

HidBridge::~HidBridge() {
    Close();
}

bool HidBridge::Open() {
    Close();

    // Query device information details for our virtual driver interface GUID
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_VIRTUAL_HID,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool found = false;
    std::wstring devicePath;

    // Enumerate interface instances (usually there is only one)
    if (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_DEVINTERFACE_VIRTUAL_HID, 0, &deviceInterfaceData)) {
        DWORD detailSize = 0;
        
        // Retrieve size first
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &deviceInterfaceData, NULL, 0, &detailSize, NULL);
        
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<uint8_t> detailBuffer(detailSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &deviceInterfaceData, detailData, detailSize, NULL, NULL)) {
                devicePath = detailData->DevicePath;
                found = true;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!found) {
        return false;
    }

    // Open connection to driver
    m_hDevice = CreateFileW(
        devicePath.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open virtual HID device connection. Error: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "Successfully connected to Virtual HID Driver!" << std::endl;
    m_firstUpdate = true;
    return true;
}

void HidBridge::Close() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool HidBridge::SendMouseReport(const GESTURE_MOUSE_REPORT& report) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL success = DeviceIoControl(
        m_hDevice,
        IOCTL_WRITE_GESTURE_INPUT,
        const_cast<GESTURE_MOUSE_REPORT*>(&report),
        sizeof(GESTURE_MOUSE_REPORT),
        NULL,
        0,
        &bytesReturned,
        NULL
    );

    return (success != 0);
}

void HidBridge::SendTrackingUpdate(const GestureOutput& output) {
    if (m_hDevice == INVALID_HANDLE_VALUE) {
        return;
    }

    if (m_firstUpdate) {
        m_lastX = output.cursorX;
        m_lastY = output.cursorY;
        m_firstUpdate = false;
        return;
    }

    // Calculate relative differences
    float dx = (output.cursorX - m_lastX);
    float dy = (output.cursorY - m_lastY);

    // Dynamic mouse sensitivity scalar (convert 0-1 coordinate diff to relative pixels)
    constexpr float SENSITIVITY = 2500.0f;
    float pixelDx = dx * SENSITIVITY;
    float pixelDy = dy * SENSITIVITY;

    // Build standard report payload
    GESTURE_MOUSE_REPORT report;
    report.Buttons = 0;
    report.Wheel = 0;

    // Map Click/Drag state
    if (output.state == State::PINCH) {
        report.Buttons |= 0x01; // Left click mouse bit
    }

    // Clamp values to signed 8-bit limits (-127 to 127)
    float clampedDx = std::max(-127.0f, std::min(127.0f, pixelDx));
    float clampedDy = std::max(-127.0f, std::min(127.0f, pixelDy));

    report.X = static_cast<int8_t>(clampedDx);
    report.Y = static_cast<int8_t>(clampedDy);

    // Send report to Virtual HID Driver
    if (!SendMouseReport(report)) {
        // Handle disconnection/errors
        std::cerr << "Failed to send driver report. Closing connection." << std::endl;
        Close();
    }

    // Update tracking states
    m_lastX = output.cursorX;
    m_lastY = output.cursorY;
}

} // namespace gesture
