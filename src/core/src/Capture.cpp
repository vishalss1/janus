#include "Capture.h"
#include <iostream>

template <class T> void SafeRelease(T **ppT) {
    if (*ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

namespace gesture {

Capture::Capture() : m_initialized(false), m_width(640), m_height(480) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    MFStartup(MF_VERSION);
}

Capture::~Capture() {
    Shutdown();
    MFShutdown();
    CoUninitialize();
}

std::vector<std::wstring> Capture::EnumerateDevices() {
    std::vector<std::wstring> devices;
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return devices;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) {
        hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    }
    SafeRelease(&pAttributes);

    if (SUCCEEDED(hr)) {
        for (UINT32 i = 0; i < count; i++) {
            WCHAR* friendlyName = nullptr;
            UINT32 nameLen = 0;
            hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &friendlyName, &nameLen);
            if (SUCCEEDED(hr) && friendlyName) {
                devices.push_back(friendlyName);
                CoTaskMemFree(friendlyName);
            }
            SafeRelease(&ppDevices[i]);
        }
        CoTaskMemFree(ppDevices);
    }
    return devices;
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

    // Set video format to RGB32 (which is BGRA 8888 internally in MF)
    IMFMediaType* pType = nullptr;
    hr = MFCreateMediaType(&pType);
    if (FAILED(hr)) return false;

    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

    hr = m_pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
    SafeRelease(&pType);

    if (FAILED(hr)) return false;

    // Retrieve the actual dynamic resolution negotiated by the camera
    IMFMediaType* pCurrentType = nullptr;
    hr = m_pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (SUCCEEDED(hr)) {
        UINT32 w = 0, h = 0;
        hr = MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &w, &h);
        if (SUCCEEDED(hr)) {
            m_width = w;
            m_height = h;
            std::cout << "Negotiated Camera Resolution: " << m_width << "x" << m_height << std::endl;
        }
        pCurrentType->Release();
    }

    return true;
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

    // Try to get 2D buffer first to handle stride correctly
    IMF2DBuffer* p2DBuffer = nullptr;
    hr = pBuffer->QueryInterface(IID_PPV_ARGS(&p2DBuffer));

    if (SUCCEEDED(hr)) {
        BYTE* pData = nullptr;
        LONG stride = 0;
        hr = p2DBuffer->Lock2D(&pData, &stride);
        if (SUCCEEDED(hr)) {
            width = m_width;
            height = m_height;
            
            // Dense copy of BGRA pixels line by line, stripping any alignment stride padding
            frameBuffer.resize(width * height * 4);
            uint8_t* destPtr = frameBuffer.data();
            uint8_t* srcPtr = pData;
            
            size_t rowBytes = width * 4;
            for (uint32_t y = 0; y < height; ++y) {
                std::copy(srcPtr, srcPtr + rowBytes, destPtr);
                destPtr += rowBytes;
                srcPtr += stride;
            }
            p2DBuffer->Unlock2D();
        }
        p2DBuffer->Release();
    } else {
        // Fall back to standard Lock if IMF2DBuffer is not supported
        BYTE* pData = nullptr;
        DWORD maxLength = 0, currentLength = 0;
        hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
        if (SUCCEEDED(hr)) {
            width = m_width;
            height = m_height;
            frameBuffer.assign(pData, pData + currentLength);
            pBuffer->Unlock();
        }
    }

    SafeRelease(&pBuffer);
    SafeRelease(&pSample);

    return SUCCEEDED(hr);
}

} // namespace gesture
