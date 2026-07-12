#pragma once

#include <windows.h>
#include "Fsm.h"

namespace gesture {

class InputInjector {
public:
    InputInjector();
    ~InputInjector();

    // Translates absolute tracking coordinates into relative mouse movements
    // and sends them along with button states and scroll wheel events via SendInput.
    void SendTrackingUpdate(const GestureOutput& output);

private:
    float m_lastX;
    float m_lastY;
    bool m_firstUpdate;
};

} // namespace gesture
