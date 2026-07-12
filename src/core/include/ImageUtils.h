#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

namespace gesture {
namespace utils {

// Resizes a BGRA (4 channels, 8-bit per channel) image to a specified size (destWidth x destHeight) 
// using bilinear interpolation, and simultaneously normalizes and converts to an RGB float interleaved tensor.
// Output tensor shape: [destHeight, destWidth, 3] (interleaved NHWC format expected by ONNX model).
inline void PreprocessImage(
    const std::vector<uint8_t>& bgraBuffer,
    uint32_t srcWidth,
    uint32_t srcHeight,
    uint32_t destWidth,
    uint32_t destHeight,
    std::vector<float>& outTensor
) {
    outTensor.resize(3 * destWidth * destHeight);
    
    float xRatio = static_cast<float>(srcWidth - 1) / destWidth;
    float yRatio = static_cast<float>(srcHeight - 1) / destHeight;

    for (uint32_t y = 0; y < destHeight; ++y) {
        for (uint32_t x = 0; x < destWidth; ++x) {
            int xL = static_cast<int>(xRatio * x);
            int yL = static_cast<int>(yRatio * y);
            int xH = std::min(xL + 1, static_cast<int>(srcWidth - 1));
            int yH = std::min(yL + 1, static_cast<int>(srcHeight - 1));

            float xWeight = (xRatio * x) - xL;
            float yWeight = (yRatio * y) - yL;

            // BGRA strides
            auto getPixel = [&](int px, int py, int channel) -> float {
                // channel: 0=B, 1=G, 2=R, 3=A
                size_t index = (static_cast<size_t>(py) * srcWidth + px) * 4;
                if (index + channel >= bgraBuffer.size()) return 0.0f;
                return static_cast<float>(bgraBuffer[index + channel]);
            };

            // Interpolate for Blue, Green, Red channels
            for (int c = 0; c < 3; ++c) {
                float a = getPixel(xL, yL, c);
                float b = getPixel(xH, yL, c);
                float clr = getPixel(xL, yH, c);
                float d = getPixel(xH, yH, c);

                float val = a * (1 - xWeight) * (1 - yWeight) +
                            b * xWeight * (1 - yWeight) +
                            clr * (1 - xWeight) * yWeight +
                            d * xWeight * yWeight;

                // Normalize to [0.0, 1.0] range
                float normVal = val / 255.0f;

                // Interleaved NHWC format: index = (y * destWidth + x) * 3
                size_t destIndex = (static_cast<size_t>(y) * destWidth + x) * 3;
                
                if (c == 2) { // Red -> offset 0
                    outTensor[destIndex + 0] = normVal;
                } else if (c == 1) { // Green -> offset 1
                    outTensor[destIndex + 1] = normVal;
                } else if (c == 0) { // Blue -> offset 2
                    outTensor[destIndex + 2] = normVal;
                }
            }
        }
    }
}

// Performs global histogram equalization in-place on a BGRA buffer
inline void EqualizeContrast(std::vector<uint8_t>& bgraBuffer, uint32_t width, uint32_t height) {
    if (bgraBuffer.empty()) return;

    // 1. Calculate intensity histogram (use green channel as a representative of IR frame)
    uint32_t histogram[256] = {0};
    size_t pixelCount = width * height;
    
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t g = bgraBuffer[i * 4 + 1];
        histogram[g]++;
    }

    // 2. Compute Cumulative Distribution Function (CDF)
    uint32_t cdf[256] = {0};
    cdf[0] = histogram[0];
    for (int i = 1; i < 256; ++i) {
        cdf[i] = cdf[i - 1] + histogram[i];
    }

    // 3. Find minimum non-zero CDF value
    uint32_t cdfMin = 0;
    for (int i = 0; i < 256; ++i) {
        if (cdf[i] > 0) {
            cdfMin = cdf[i];
            break;
        }
    }

    uint32_t totalPixels = cdf[255];
    if (totalPixels - cdfMin == 0) {
        return; // Uniform image, nothing to equalize
    }

    // 4. Create lookup table (LUT)
    uint8_t lut[256] = {0};
    for (int i = 0; i < 256; ++i) {
        float val = static_cast<float>(cdf[i] - cdfMin) / (totalPixels - cdfMin) * 255.0f;
        lut[i] = static_cast<uint8_t>(val < 0.0f ? 0.0f : (val > 255.0f ? 255.0f : val));
    }

    // 5. Map channels in-place using lookup table
    for (size_t i = 0; i < pixelCount; ++i) {
        bgraBuffer[i * 4 + 0] = lut[bgraBuffer[i * 4 + 0]]; // Blue
        bgraBuffer[i * 4 + 1] = lut[bgraBuffer[i * 4 + 1]]; // Green
        bgraBuffer[i * 4 + 2] = lut[bgraBuffer[i * 4 + 2]]; // Red
    }
}

} // namespace utils
} // namespace gesture
