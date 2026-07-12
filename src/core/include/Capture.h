#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

namespace gesture {

class Capture {
public:
    Capture();
    ~Capture();

    bool Initialize(uint32_t deviceIndex = 0);
    void Shutdown();

    // Pull next video frame. Returns raw BGRA or RGB bytes.
    bool GrabFrame(std::vector<uint8_t>& frameBuffer, uint32_t& width, uint32_t& height);

private:
    bool SetupSourceReader(IMFActivate* pActivate);

    Microsoft::WRL::ComPtr<IMFSourceReader> m_pReader;
    bool m_initialized;
};

} // namespace gesture
