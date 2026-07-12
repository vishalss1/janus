#include "Inference.h"
#include "ImageUtils.h"
#include <iostream>
#include <exception>
#include <cmath>

#ifdef WITH_DIRECTML
// DirectML header might be required if we use DML EP
#include <dml_provider_factory.h>
#endif

namespace gesture {

Inference::Inference() : m_modelLoaded(false) {}

Inference::~Inference() {}

bool Inference::LoadModel(const std::wstring& modelPath) {
#ifdef WITH_ONNXRUNTIME
    try {
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "GestureDevice");
        
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(2);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef WITH_DIRECTML
        // Append DirectML Execution Provider (device index 0)
        HRESULT hr = OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0);
        if (FAILED(hr)) {
            std::cerr << "DirectML Execution Provider could not be initialized. Falling back to CPU." << std::endl;
        } else {
            std::cout << "Successfully appended DirectML Execution Provider." << std::endl;
        }
#else
        std::cout << "DirectML Execution Provider disabled. Using default CPU Execution Provider." << std::endl;
#endif

        m_session = std::make_unique<Ort::Session>(*m_env, modelPath.c_str(), sessionOptions);
        
        // Retrieve input/output nodes
        Ort::AllocatorWithDefaultOptions allocator;
        size_t numInputNodes = m_session->GetInputCount();
        for (size_t i = 0; i < numInputNodes; i++) {
            auto inputName = m_session->GetInputNameAllocated(i, allocator);
            m_inputNodeNames.push_back(inputName.get());
        }

        size_t numOutputNodes = m_session->GetOutputCount();
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto outputName = m_session->GetOutputNameAllocated(i, allocator);
            m_outputNodeNames.push_back(outputName.get());
        }

        std::wcout << L"Loaded ONNX model from: " << modelPath << L" using ONNX Runtime." << std::endl;
        m_modelLoaded = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ONNX Runtime model load failed: " << e.what() << std::endl;
        m_modelLoaded = false;
        return false;
    }
#else
    std::wcout << L"[STUB] Loading model: " << modelPath << L" (Build without ONNX Runtime for stub mode)" << std::endl;
    m_modelLoaded = true;
    return true;
#endif
}

bool Inference::RunInference(const std::vector<uint8_t>& imageBuffer, uint32_t width, uint32_t height, std::vector<Landmark>& outLandmarks, bool enableIrMode) {
    if (!m_modelLoaded) return false;

    outLandmarks.clear();

#ifdef WITH_ONNXRUNTIME
    try {
        // Preprocess BGRA to 224x224 RGB planar float tensor
        uint32_t modelInputSize = 224; // Standard size for many MediaPipe hand landmark models
        std::vector<float> inputTensorValues;
        
        std::vector<uint8_t> processedBuffer;
        const std::vector<uint8_t>* bufferToPreprocess = &imageBuffer;
        
        if (enableIrMode) {
            processedBuffer = imageBuffer;
            utils::EqualizeContrast(processedBuffer, width, height);
            bufferToPreprocess = &processedBuffer;
        }

        utils::PreprocessImage(
            *bufferToPreprocess,
            width,
            height,
            modelInputSize,
            modelInputSize,
            inputTensorValues
        );

        // Define input shapes: [1, 224, 224, 3] (NHWC)
        std::vector<int64_t> inputDims = {1, modelInputSize, modelInputSize, 3};

        // Create Ort input tensor
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo,
            inputTensorValues.data(),
            inputTensorValues.size(),
            inputDims.data(),
            inputDims.size()
        );

        // Vector of input/output names
        std::vector<const char*> inputNames;
        for (const auto& name : m_inputNodeNames) {
            inputNames.push_back(name.c_str());
        }
        std::vector<const char*> outputNames;
        for (const auto& name : m_outputNodeNames) {
            outputNames.push_back(name.c_str());
        }

        // Run inference session
        auto outputTensors = m_session->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(),
            &inputTensor,
            1,
            outputNames.data(),
            outputNames.size()
        );

        // Extract hand landmark output (usually output index 0 contains 21 landmarks * 3 coordinates)
        if (outputTensors.empty()) return false;

        float* floatValues = outputTensors[0].GetTensorMutableData<float>();
        auto tensorInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
        size_t elementCount = tensorInfo.GetElementCount();

        // Expecting 21 landmarks * 3 values (x, y, z) = 63 values
        if (elementCount >= 63) {
            outLandmarks.resize(21);
            for (int i = 0; i < 21; ++i) {
                // Normalize coordinates from 224x224 pixel-space to [0.0, 1.0] normalized-space
                outLandmarks[i].x = floatValues[i * 3 + 0] / 224.0f;
                outLandmarks[i].y = floatValues[i * 3 + 1] / 224.0f;
                outLandmarks[i].z = floatValues[i * 3 + 2] / 224.0f;
            }
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Inference execution failed: " << e.what() << std::endl;
        return false;
    }
#else
    // Mock landmarks for stub mode testing (simulates an open hand drifting in a circle)
    static float angle = 0.0f;
    angle += 0.03f;
    float offsetX = 0.05f * std::cos(angle);
    float offsetY = 0.05f * std::sin(angle);

    outLandmarks.resize(21);
    for (int i = 0; i < 21; ++i) {
        outLandmarks[i].x = 0.5f + (i * 0.01f) + offsetX;
        outLandmarks[i].y = 0.5f - (i * 0.01f) + offsetY;
        outLandmarks[i].z = 0.0f;
    }
    return true;
#endif
}

} // namespace gesture
