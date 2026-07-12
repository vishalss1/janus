#include "Capture.h"
#include "Inference.h"
#include "Fsm.h"
#include "SharedMemory.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Starting Gesture Input Core Pipeline..." << std::endl;

    // 1. Initialize Shared Memory for IPC
    HANDLE hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        gesture::GESTURE_SHARED_MEMORY_SIZE, // maximum object size (low-order DWORD)
        gesture::GESTURE_SHARED_MEMORY_NAME  // name of mapping object
    );

    if (hMapFile == NULL) {
        std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
        return 1;
    }

    gesture::SharedMemoryLayout* pBuf = (gesture::SharedMemoryLayout*)MapViewOfFile(
        hMapFile,            // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        gesture::GESTURE_SHARED_MEMORY_SIZE
    );

    if (pBuf == NULL) {
        std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // Initialize buffer state
    pBuf->magic = 0x48414E44; // "HAND"
    pBuf->sequenceNumber = 0;
    pBuf->handDetected = false;

    // 2. Initialize Camera Capture
    gesture::Capture capture;
    if (!capture.Initialize(0)) {
        std::cerr << "Failed to initialize Media Foundation capture." << std::endl;
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return 1;
    }
    std::cout << "Camera initialized." << std::endl;

    // 3. Initialize Model Inference
    gesture::Inference inference;
    // Assume relative path to model for initial setup
    if (!inference.LoadModel(L"models/mediapipe_hand.onnx")) {
        std::cerr << "Failed to initialize inference engine." << std::endl;
        capture.Shutdown();
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return 1;
    }

    // 4. Initialize Gesture FSM
    gesture::Fsm fsm;
    std::cout << "Gesture FSM initialized. Press Ctrl+C to exit." << std::endl;

    // 5. Run Main Pipeline Loop
    std::vector<uint8_t> frameBuffer;
    uint32_t width = 0, height = 0;
    std::vector<gesture::Landmark> landmarks;

    while (true) {
        if (capture.GrabFrame(frameBuffer, width, height)) {
            // Run ONNX Landmark model
            if (inference.RunInference(frameBuffer, width, height, landmarks)) {
                // Update FSM state machine
                fsm.Update(landmarks);

                // Update Shared Memory for overlay UI
                pBuf->sequenceNumber++;
                pBuf->handDetected = !landmarks.empty();
                
                if (pBuf->handDetected) {
                    pBuf->activeState = static_cast<uint32_t>(fsm.GetCurrentState());
                    // Set index tip as cursor position
                    pBuf->cursorX = landmarks[8].x;
                    pBuf->cursorY = landmarks[8].y;
                    
                    // Copy all 21 landmarks
                    for (int i = 0; i < 21; ++i) {
                        pBuf->landmarks[i].x = landmarks[i].x;
                        pBuf->landmarks[i].y = landmarks[i].y;
                        pBuf->landmarks[i].z = landmarks[i].z;
                    }
                }
            }
        }
        
        // Throttling to run around 60hz for low CPU overhead
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Clean up
    capture.Shutdown();
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    return 0;
}
