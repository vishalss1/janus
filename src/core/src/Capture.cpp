#include "Capture.h"
#include <iostream>

// Helper to release COM pointers
template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

namespace gesture {

Capture::Capture() : m_initialized(false) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    MFStartup(MF_VERSION);
}

Capture::~Capture() {
    Shutdown();
    MFShutdown();
    CoUninitialize();
}

bool Capture::Initialize(uint32_t deviceIndex) {
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return false;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        SafeRelease(&pAttributes);
        return false;
    }

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    SafeRelease(&pAttributes);
    if (FAILED(hr) || count == 0) {
        std::cerr << "No video capture devices found." << std::endl;
        return false;
    }

    if (deviceIndex >= count) {
        std::cerr << "Device index out of range. Found " << count << " devices." << std::endl;
        for (UINT32 i = 0; i < count; i++) SafeRelease(&ppDevices[i]);
        CoTaskMemFree(ppDevices);
        return false;
    }

    bool success = SetupSourceReader(ppDevices[deviceIndex]);

    for (UINT32 i = 0; i < count; i++) SafeRelease(&ppDevices[i]);
    CoTaskMemFree(ppDevices);

    m_initialized = success;
    return success;
}

bool Capture::SetupSourceReader(IMFActivate* pActivate) {
    IMFMediaSource* pSource = nullptr;
    HRESULT hr = pActivate->ActivateObject(IID_PPV_ARGS(&pSource));
    if (FAILED(hr)) return false;

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 2);
    if (SUCCEEDED(hr)) {
        pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);
        pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        hr = MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &m_pReader);
        SafeRelease(&pAttributes);
    }

    SafeRelease(&pSource);
    if (FAILED(hr)) return false;

    // Set video format to RGB32
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(mt));
    mt.majortype = MFMediaType_Video;
    mt.subtype = MFVideoFormat_RGB32;

    hr = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, &mt);
    return SUCCEEDED(hr);
}

void Capture::Shutdown() {
    m_pReader.Reset();
    m_initialized = false;
}

bool Capture::GrabFrame(std::vector<uint8_t>& frameBuffer, uint32_t& width, uint32_t& height) {
    if (!m_initialized || !m_pReader) return false;

    DWORD streamIndex, flags;
    LONGLONG timestamp;
    IMFSample* pSample = nullptr;

    HRESULT hr = m_pReader->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &flags,
        &timestamp,
        &pSample
    );

    if (FAILED(hr) || !pSample) {
        SafeRelease(&pSample);
        return false;
    }

    IMFMediaBuffer* pBuffer = nullptr;
    hr = pSample->GetBufferByIndex(0, &pBuffer);
    if (FAILED(hr)) {
        SafeRelease(&pSample);
        return false;
    }

    BYTE* pData = nullptr;
    DWORD maxLength = 0, currentLength = 0;
    hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
    if (SUCCEEDED(hr)) {
        // Assume dummy values for width/height in this stub
        // Actual media type should be queried from source reader in production
        width = 640;
        height = 480;

        frameBuffer.assign(pData, pData + currentLength);
        pBuffer->Unlock();
    }

    SafeRelease(&pBuffer);
    SafeRelease(&pSample);

    return SUCCEEDED(hr);
}

} // namespace gesture
