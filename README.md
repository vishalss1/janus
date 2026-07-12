# Gesture Input Device (Windows)

A camera-driven hand tracking solution that presents itself to Windows as a real HID input device (virtual mouse + keyboard), complete with a visual feedback overlay showing hand positions.

This is a native, daily-use Windows application designed for low latency, low overhead, and direct driver-level interaction.

## Architecture Overview

```
Camera (Media Foundation)
   │
   ▼
Core Native Pipeline (C++ / ONNX Runtime + DirectML)
   │
   ├─► Shared Memory (Hand landmarks & FSM state) ──► Overlay UI (Direct2D/WPF)
   │
   ▼ (DeviceIoControl)
UMDF2 Virtual HID Driver (Windows WDK)
   │
   ▼
Windows HID Stack (OS processes as standard mouse/keyboard)
```

## Directory Structure

- `src/prototype/` (Phase 0) - Python prototype for testing landmark accuracy, gesture sets, and FSM debounce values.
- `src/core/` (Phase 1) - Core C++ pipeline containing Media Foundation capture, ONNX Runtime inference (with DirectML execution provider), and gesture FSM.
- `src/driver/` (Phase 2) - Windows User-Mode Driver Framework (UMDF2) virtual HID driver based on the Microsoft HIDMiniDriver.
- `src/overlay/` (Phase 3) - User overlay rendering hand landmarks and state in a click-through transparent topmost window.

## Build Phases

Refer to [CLAUDE.md](CLAUDE.md) for detailed descriptions of the 5 phases of construction:
1. **Phase 0 — Prototype**: Setup Python env, OpenCV, MediaPipe, pyautogui.
2. **Phase 1 — Core Native Pipeline**: C++ application, CMake, Media Foundation, ONNX Runtime.
3. **Phase 2 — Virtual HID Driver**: UMDF2 Driver, WDK.
4. **Phase 3 — Overlay**: Transparent layered window showing hand feedback.
5. **Phase 4 — IR / Low-light**: Dark mode sensing via modified IR camera.
6. **Phase 5 — Integration + Hardening**: Driver crash recovery, false-gesture tuning, performance profiling.
