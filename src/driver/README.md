# Phase 2 — UMDF2 Virtual HID Driver

This directory contains the WDK (Windows Driver Kit) skeleton for the User-Mode Driver Framework 2 (UMDF2) virtual HID driver. The driver enables the C++ pipeline to emulate standard HID mouse and keyboard inputs at the kernel level without using synthetic API injection (`SendInput`).

## Project Files

- `Driver.h` / `Driver.c` - Driver entry points and configuration.
- `Device.h` / `Device.c` - UMDF2 device creation, configuration of the `GUID_DEVINTERFACE_VIRTUAL_HID` interface.
- `Queue.h` / `Queue.c` - Driver IO queue processing. Handles standard HID requests (`IOCTL_HID_GET_DEVICE_DESCRIPTOR`, `IOCTL_HID_GET_REPORT_DESCRIPTOR`) and our custom `IOCTL_WRITE_GESTURE_INPUT` from the pipeline.
- `virtual_hid.inf` - Driver package installation definition.

## Building the Driver

1. Install **Visual Studio 2022** with the **C++ Desktop Development** workload.
2. Install the **Windows 11 SDK** and the **Windows Driver Kit (WDK)**.
3. Open Visual Studio and create a new **User Mode Driver, Empty (UMDF 2)** project.
4. Add the C files (`Driver.c`, `Device.c`, `Queue.c`) and header files to the project.
5. Set target platform settings to `x64` and build.

## Installation & Testing

To load user-mode drivers on a local development machine, Windows must be in **Test Signing Mode**:

1. Open PowerShell or Command Prompt as Administrator.
2. Enable testsigning mode:
   ```cmd
   bcdedit /set testsigning on
   ```
3. Restart your computer.
4. Register the driver package using `devcon.exe` (included in WDK) or Device Manager:
   ```cmd
   devcon install virtual_hid.inf UMDF\VirtualHidDevice
   ```

## Application Integration

The C++ Core application can open a handle to this driver by calling `CreateFile` with the symbolic link retrieved via device interface `GUID_DEVINTERFACE_VIRTUAL_HID`.
It writes inputs using `DeviceIoControl` and `IOCTL_WRITE_GESTURE_INPUT`.
