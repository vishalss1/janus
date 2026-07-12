#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace gesture {

struct Landmark {
    float x;
    float y;
    float z;
};

class Inference {
public:
    Inference();
    ~Inference();

    bool LoadModel(const std::wstring& modelPath);
    
    // Run hand landmark model on raw image buffer (assumes RGB24 or RGBA format)
    bool RunInference(const std::vector<uint8_t>& imageBuffer, uint32_t width, uint32_t height, std::vector<Landmark>& outLandmarks);

private:
    bool m_modelLoaded;
    // Pointers for ONNX Runtime environment and sessions will go here once ONNXRT is linked
};

} // namespace gesture
