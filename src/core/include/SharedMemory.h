#pragma once

#include <windows.h>
#include <cstdint>

namespace gesture {

// Struct containing shared memory layout
// Shared by Core Native Pipeline and Overlay UI process
struct SharedMemoryLayout {
    // Unique identifier/version for format validation
    uint32_t magic;            // e.g. 0x48414E44 ("HAND")
    uint32_t layoutVersion;    // Version stamp of layout (e.g. 1)
    uint32_t sequenceNumber;   // Incrementing counter per frame update
    
    // Status flags
    bool handDetected;
    bool pinchActive;
    uint32_t activeState;      // Current active FSM state index (HOVER=0, PINCH=1, FIST=2, etc.)
    uint32_t swipeDirection;   // Direction of swipe (NONE=0, LEFT=1, RIGHT=2, UP=4, DOWN=8)
    
    // Cursor position (normalized screen space [0, 1])
    float cursorX;
    float cursorY;

    // Hand coordinates (21 landmarks)
    struct {
        float x;
        float y;
        float z;
    } landmarks[21];
};

constexpr const wchar_t* GESTURE_SHARED_MEMORY_NAME = L"Local\\GestureInputDeviceSharedMemory";
constexpr size_t GESTURE_SHARED_MEMORY_SIZE = sizeof(SharedMemoryLayout);

} // namespace gesture
