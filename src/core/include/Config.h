#pragma once

#include <string>
#include <cstdint>

namespace gesture {

struct Config {
    uint32_t debounceMs = 150;
    float pinchEnterDist = 0.05f;
    float pinchExitDist = 0.08f;
    float swipeVelocityThreshold = 0.04f;
    uint32_t swipeWindowFrames = 8;
    uint32_t cameraDeviceIndex = 0;
    uint32_t targetFps = 30;
    std::wstring modelPath = L"models/handpose_estimation_mediapipe_2023feb.onnx";
    bool enableIrMode = false;

    // Load from a simple key=value text file (e.g. config.txt).
    // If file doesn't exist, writes a default one and returns true.
    bool Load(const std::wstring& filePath);
    
    // Save current config values to the file.
    bool Save(const std::wstring& filePath) const;
};

} // namespace gesture
