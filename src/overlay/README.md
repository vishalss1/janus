# Phase 3 — Overlay UI

The Overlay UI is a separate, cosmetic-only process that runs in parallel to the core C++ native pipeline. It reads hand landmarks and FSM states from the Shared Memory buffer (`Local\\GestureInputDeviceSharedMemory`) and renders visual feedback directly onto the screen.

## Requirements

To prevent input latency, the overlay process:
1. Must not run on the hot tracking path (the C++ process runs independently).
2. Must be click-through and completely transparent.
3. Must remain top-most on the Windows Z-order.

## Implementation Options

You can build the overlay in either of these styles:

### Option A: WPF (C# / .NET) - Recommended for rapid UI iteration
- **Window setup**: Use a `Window` with `AllowsTransparency="True"`, `WindowStyle="None"`, `Background="Transparent"`, and `Topmost="True"`.
- **Click-through**: Call Windows API `SetWindowLong` to apply `WS_EX_TRANSPARENT` and `WS_EX_LAYERED` styles.
- **Shared Memory**: Use C# `System.IO.MemoryMappedFiles.MemoryMappedFile` to open `Local\\GestureInputDeviceSharedMemory` and map it to a struct matching `SharedMemoryLayout`.
- **Render loop**: Use `CompositionTarget.Rendering` to poll shared memory and redraw landmarks on a Canvas.

### Option B: Direct2D (C++) - Recommended for ultra-low memory footprints
- **Window setup**: Standard Win32 window with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST` styles.
- **Rendering**: Initialize a Direct2D render target using `ID2D1HwndRenderTarget`.
- **Shared Memory**: `OpenFileMappingW` and `MapViewOfFile` to read the state.
- **Render loop**: Classic Win32 message loop drawing hand skeletal lines using `DrawLine` and circles for keypoints.

## Shared Memory Layout Reference

Refer to the C++ struct definition in [SharedMemory.h](file:///d:/janus/src/core/include/SharedMemory.h):
- `magic`: Validated against `0x48414E44` ("HAND").
- `sequenceNumber`: Incrementing counter (overlay can skip rendering if sequence number hasn't changed).
- `handDetected`: True when landmarks are valid.
- `cursorX` / `cursorY`: Calculated tracking position.
- `landmarks`: Array of 21 (x, y, z) coordinates.
