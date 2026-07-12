#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

#ifdef WITH_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

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
    
    // Run hand landmark model on raw image buffer (BGRA format)
    bool RunInference(const std::vector<uint8_t>& imageBuffer, uint32_t width, uint32_t height, std::vector<Landmark>& outLandmarks);

private:
    bool m_modelLoaded;

#ifdef WITH_ONNXRUNTIME
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    std::vector<std::string> m_inputNodeNames;
    std::vector<std::string> m_outputNodeNames;
#endif
};

} // namespace gesture
