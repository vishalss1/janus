#pragma once

#include "SharedMemory.h"
#include <windows.h>
#include <iostream>

namespace gesture {

class SharedMemoryWriter {
public:
    SharedMemoryWriter() : m_hMapFile(NULL), m_pBuf(nullptr) {}

    ~SharedMemoryWriter() {
        Close();
    }

    bool Open(const wchar_t* name = GESTURE_SHARED_MEMORY_NAME) {
        m_hMapFile = CreateFileMappingW(
            INVALID_HANDLE_VALUE,    // Use paging file
            NULL,                    // Default security
            PAGE_READWRITE,          // Read/write access
            0,                       // Max size high
            static_cast<DWORD>(GESTURE_SHARED_MEMORY_SIZE), // Max size low
            name                     // Name of mapping
        );

        if (m_hMapFile == NULL) {
            std::cerr << "Failed to create shared memory mapping: " << GetLastError() << std::endl;
            return false;
        }

        m_pBuf = MapViewOfFile(
            m_hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            GESTURE_SHARED_MEMORY_SIZE
        );

        if (m_pBuf == nullptr) {
            std::cerr << "Failed to map view of shared memory: " << GetLastError() << std::endl;
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
            return false;
        }

        // Initialize mapping default values
        auto* data = Data();
        data->magic = 0x48414E44; // "HAND"
        data->layoutVersion = 1;
        data->sequenceNumber = 0;
        data->handDetected = false;
        data->pinchActive = false;
        data->activeState = 0;
        data->swipeDirection = 0;
        data->cursorX = 0.5f;
        data->cursorY = 0.5f;
        
        return true;
    }

    void Close() {
        if (m_pBuf) {
            UnmapViewOfFile(m_pBuf);
            m_pBuf = nullptr;
        }
        if (m_hMapFile) {
            CloseHandle(m_hMapFile);
            m_hMapFile = NULL;
        }
    }

    SharedMemoryLayout* Data() {
        return reinterpret_cast<SharedMemoryLayout*>(m_pBuf);
    }

private:
    HANDLE m_hMapFile;
    void* m_pBuf;
};

} // namespace gesture
