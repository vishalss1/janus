#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

namespace gesture {
namespace utils {

// Resizes a BGRA (4 channels, 8-bit per channel) image to a specified size (destWidth x destHeight) 
// using bilinear interpolation, and simultaneously normalizes and converts to an RGB float planar tensor.
// Output tensor shape: [3, destHeight, destWidth] (planar format expected by ONNX model).
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

    float* rChannel = outTensor.data();
    float* gChannel = outTensor.data() + (destWidth * destHeight);
    float* bChannel = outTensor.data() + (2 * destWidth * destHeight);

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

                size_t destIndex = static_cast<size_t>(y) * destWidth + x;
                
                // Map output to planar RGB format (model expectation)
                if (c == 2) { // Red
                    rChannel[destIndex] = normVal;
                } else if (c == 1) { // Green
                    gChannel[destIndex] = normVal;
                } else if (c == 0) { // Blue
                    bChannel[destIndex] = normVal;
                }
            }
        }
    }
}

} // namespace utils
} // namespace gesture
