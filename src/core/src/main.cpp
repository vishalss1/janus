#include "Capture.h"
#include "Inference.h"
#include "Fsm.h"
#include "Config.h"
#include "SharedMemoryWriter.h"
#include "InputInjector.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

std::atomic<bool> g_running(true);

// Frame queue variables
std::mutex g_frameMutex;
std::condition_variable g_frameCv;
std::vector<uint8_t> g_pendingFrame;
uint32_t g_pendingWidth = 0;
uint32_t g_pendingHeight = 0;
bool g_frameReady = false;

// Windows Control Handler for Clean Exit
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\nShutdown signal received. Cleaning up..." << std::endl;
        g_running = false;
        g_frameCv.notify_all();
        return TRUE;
    }
    return FALSE;
}

// Phase 1 Automated Test / Smoke Test Mode
int RunSmokeTest() {
    std::cout << "[SMOKE TEST] Initializing FSM test cases..." << std::endl;
    gesture::Fsm fsm;
    gesture::Config config; // default configs

    // Helper to verify state transitions
    auto verifyState = [&](const std::vector<gesture::Landmark>& input, gesture::State expectedState, const std::string& caseName) -> bool {
        // Feed the same input several times to exceed debounceMs (150ms)
        for (int i = 0; i < 15; ++i) {
            fsm.Update(input, config);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        
        auto currentState = fsm.GetOutput().state;
        if (currentState == expectedState) {
            std::cout << "  [PASS] " << caseName << " transitioned to " << fsm.StateToString(currentState) << std::endl;
            return true;
        } else {
            std::cout << "  [FAIL] " << caseName << " (Expected " << fsm.StateToString(expectedState) 
                      << ", got " << fsm.StateToString(currentState) << ")" << std::endl;
            return false;
        }
    };

    bool allPassed = true;

    // Test Case 1: Open Hand (Hover)
    std::vector<gesture::Landmark> hoverHand(21);
    // Position fingers far from wrist (landmark 0)
    for (int i = 0; i < 21; ++i) {
        hoverHand[i].x = 0.5f + (i * 0.05f);
        hoverHand[i].y = 0.5f - (i * 0.05f);
        hoverHand[i].z = 0.0f;
    }
    allPassed &= verifyState(hoverHand, gesture::State::HOVER, "Hover Gesture Case");

    // Test Case 2: Pinch (Thumb [4] and Index [8] tips close)
    std::vector<gesture::Landmark> pinchHand = hoverHand;
    pinchHand[4] = {0.5f, 0.5f, 0.0f}; // Thumb tip
    pinchHand[8] = {0.501f, 0.501f, 0.0f}; // Index tip (dist ~0.0014 < enterDist 0.05)
    allPassed &= verifyState(pinchHand, gesture::State::PINCH, "Pinch Gesture Case");

    // Test Case 3: Fist (All fingers collapsed near wrist)
    std::vector<gesture::Landmark> fistHand(21);
    for (int i = 0; i < 21; ++i) {
        fistHand[i] = {0.1f, 0.1f, 0.0f}; // Collapse everything near wrist (0.0, 0.0)
    }
    allPassed &= verifyState(fistHand, gesture::State::FIST, "Fist Gesture Case");

    if (allPassed) {
        std::cout << "[SMOKE TEST] All gesture tests PASSED successfully." << std::endl;
        return 0;
    } else {
        std::cerr << "[SMOKE TEST] Some gesture tests FAILED." << std::endl;
        return 1;
    }
}

// Background Thread: Camera Capture
void CaptureThread(uint32_t deviceIndex) {
    gesture::Capture capture;
    if (!capture.Initialize(deviceIndex)) {
        std::cerr << "[Capture] Failed to initialize camera index " << deviceIndex << std::endl;
        g_running = false;
        g_frameCv.notify_all();
        return;
    }

    std::vector<uint8_t> frameBuffer;
    uint32_t width = 0, height = 0;

    std::cout << "[Capture] Media Foundation capture thread started." << std::endl;

    while (g_running) {
        if (capture.GrabFrame(frameBuffer, width, height)) {
            std::lock_guard<std::mutex> lock(g_frameMutex);
            g_pendingFrame = std::move(frameBuffer);
            g_pendingWidth = width;
            g_pendingHeight = height;
            g_frameReady = true;
            g_frameCv.notify_one();
        }
        
        // Brief sleep to avoid hogging CPU while polling camera
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    capture.Shutdown();
    std::cout << "[Capture] Capture thread shut down." << std::endl;
}

// Main Thread: Inference Pipeline
int main(int argc, char* argv[]) {
    // Check for smoke-test flag
    if (argc > 1 && std::string(argv[1]) == "--test-stub") {
        return RunSmokeTest();
    }

    std::cout << "===========================================" << std::endl;
    std::cout << "Starting Gesture Input Core Pipeline (Phase 1)" << std::endl;
    std::cout << "===========================================" << std::endl;

    // Register Win32 Console Handler
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "Warning: Could not set console control handler." << std::endl;
    }

    // 1. Load Configurations
    gesture::Config config;
    const std::wstring configPath = L"config.txt";
    if (config.Load(configPath)) {
        std::wcout << L"Loaded configurations from " << configPath << std::endl;
    } else {
        std::cerr << "Warning: Failed to load config.txt, using defaults." << std::endl;
    }

    // 2. Initialize Shared Memory Writer (IPC)
    gesture::SharedMemoryWriter shmWriter;
    if (!shmWriter.Open()) {
        std::cerr << "Initialization error: Could not open shared memory mapping." << std::endl;
        return 1;
    }
    std::cout << "IPC Shared Memory segment ready." << std::endl;

    // 3. Initialize Input Injector
    gesture::InputInjector injector;

    // 4. Load Inference Model
    gesture::Inference inference;
    if (!inference.LoadModel(config.modelPath)) {
        std::cerr << "Warning: Failed to load landmark model. Running in stub inference mode." << std::endl;
    }

    // 5. Initialize Gesture FSM
    gesture::Fsm fsm;

    // 6. Launch Camera Capture Thread
    std::thread capThread(CaptureThread, config.cameraDeviceIndex);

    // 7. Core Process Pipeline Loop
    std::vector<uint8_t> frameBuffer;
    uint32_t width = 0, height = 0;
    std::vector<gesture::Landmark> landmarks;

    // Timing tracking
    auto lastFrameTime = std::chrono::steady_clock::now();
    const double frameTimeThresholdMs = 1000.0 / config.targetFps;

    while (g_running) {
        // Wait for a fresh frame from the capture thread
        {
            std::unique_lock<std::mutex> lock(g_frameMutex);
            g_frameCv.wait(lock, [] { return g_frameReady || !g_running; });

            if (!g_running) break;

            frameBuffer = std::move(g_pendingFrame);
            width = g_pendingWidth;
            height = g_pendingHeight;
            g_frameReady = false;
        }

        // Maintain target frame rate (e.g. 30 fps)
        auto now = std::chrono::steady_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
        if (elapsedMs < frameTimeThresholdMs) {
            continue;
        }
        lastFrameTime = now;

        // Run landmark inference on GPU (DirectML) or CPU
        if (inference.RunInference(frameBuffer, width, height, landmarks, config.enableIrMode)) {
            // Update Debounce state FSM
            fsm.Update(landmarks, config);
            const auto& output = fsm.GetOutput();

            // Inject mouse/keyboard events via SendInput
            injector.SendTrackingUpdate(output);

            // Write outputs to IPC shared memory
            auto* shmData = shmWriter.Data();
            if (shmData) {
                shmData->sequenceNumber++;
                shmData->handDetected = !landmarks.empty();
                
                if (shmData->handDetected) {
                    shmData->pinchActive = (output.state == gesture::State::PINCH);
                    shmData->activeState = static_cast<uint32_t>(output.state);
                    shmData->swipeDirection = output.swipeDirection;
                    shmData->cursorX = output.cursorX;
                    shmData->cursorY = output.cursorY;

                    for (int i = 0; i < 21; ++i) {
                        shmData->landmarks[i].x = landmarks[i].x;
                        shmData->landmarks[i].y = landmarks[i].y;
                        shmData->landmarks[i].z = landmarks[i].z;
                    }
                }
            }
        }
    }

    // Shut down gracefully
    g_running = false;
    g_frameCv.notify_all();
    if (capThread.joinable()) {
        capThread.join();
    }

    shmWriter.Close();
    
    std::cout << "Pipeline terminated cleanly." << std::endl;
    return 0;
}
