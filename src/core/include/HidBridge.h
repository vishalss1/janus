#pragma once

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include "Fsm.h"


namespace gesture {

// Custom IOCTL for sending mouse reports to the virtual driver
#define IOCTL_WRITE_GESTURE_INPUT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// Mouse report structure matching the driver
#pragma pack(push, 1)
struct GESTURE_MOUSE_REPORT {
    uint8_t Buttons; // Bit 0: Left, Bit 1: Right, Bit 2: Middle
    int8_t  X;       // Relative movement X (-127 to 127)
    int8_t  Y;       // Relative movement Y (-127 to 127)
    int8_t  Wheel;   // Wheel movement (-127 to 127)
};
#pragma pack(pop)

class HidBridge {
public:
    HidBridge();
    ~HidBridge();

    // Locates the virtual HID driver device interface and opens a handle.
    // Gracefully returns false if driver is not loaded.
    bool Open();
    
    // Closes handle to driver.
    void Close();

    // Sends mouse movement report to the driver.
    bool SendMouseReport(const GESTURE_MOUSE_REPORT& report);

    // Checks if the driver connection is open.
    bool IsOpen() const { return m_hDevice != INVALID_HANDLE_VALUE; }

    // Translates absolute tracking coordinates into relative mouse movements and sends it.
    void SendTrackingUpdate(const GestureOutput& output);

private:
    HANDLE m_hDevice;
    float m_lastX;
    float m_lastY;
    bool m_firstUpdate;
};

} // namespace gesture
