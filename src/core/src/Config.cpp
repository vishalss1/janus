#include "Config.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>


namespace gesture {

static std::string Trim(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    return (start < end) ? std::string(start, end) : "";
}

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

bool Config::Load(const std::wstring& filePath) {
    std::ifstream file(WStringToString(filePath));
    if (!file.is_open()) {
        std::wcout << L"Config file not found. Creating default: " << filePath << std::endl;
        return Save(filePath);
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue; // Skip comments and empty lines
        }

        std::stringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value)) {
            key = Trim(key);
            value = Trim(value);

            if (key == "debounceMs") {
                debounceMs = std::stoul(value);
            } else if (key == "pinchEnterDist") {
                pinchEnterDist = std::stof(value);
            } else if (key == "pinchExitDist") {
                pinchExitDist = std::stof(value);
            } else if (key == "swipeVelocityThreshold") {
                swipeVelocityThreshold = std::stof(value);
            } else if (key == "swipeWindowFrames") {
                swipeWindowFrames = std::stoul(value);
            } else if (key == "cameraDeviceIndex") {
                cameraDeviceIndex = std::stoul(value);
            } else if (key == "targetFps") {
                targetFps = std::stoul(value);
            } else if (key == "modelPath") {
                modelPath = StringToWString(value);
            }
        }
    }
    return true;
}

bool Config::Save(const std::wstring& filePath) const {
    std::ofstream file(WStringToString(filePath));
    if (!file.is_open()) {
        std::cerr << "Failed to write config file." << std::endl;
        return false;
    }

    file << "# Gesture Input Device Configuration\n\n";
    file << "debounceMs=" << debounceMs << "\n";
    file << "pinchEnterDist=" << pinchEnterDist << "\n";
    file << "pinchExitDist=" << pinchExitDist << "\n";
    file << "swipeVelocityThreshold=" << swipeVelocityThreshold << "\n";
    file << "swipeWindowFrames=" << swipeWindowFrames << "\n";
    file << "cameraDeviceIndex=" << cameraDeviceIndex << "\n";
    file << "targetFps=" << targetFps << "\n";
    file << "modelPath=" << WStringToString(modelPath) << "\n";

    return true;
}

} // namespace gesture
