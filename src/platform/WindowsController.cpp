#include "PlatformController.h"

#ifdef _WIN32
#include <windows.h>

void PlatformController::sendKeyPress(int key_code) {
    keybd_event(key_code, 0, 0, 0);
    Sleep(50);
    keybd_event(key_code, 0, KEYEVENTF_KEYUP, 0);
}

void PlatformController::moveMouse(int x, int y) {
    SetCursorPos(x, y);
}

#endif
