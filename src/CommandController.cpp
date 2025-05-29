#include "CommandController.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#endif

CommandController::CommandController() : command_active(false) {
}

void CommandController::activateCommandMode() {
    command_active = true;
    activation_time = std::chrono::steady_clock::now();
}

void CommandController::deactivateCommandMode() {
    command_active = false;
}

bool CommandController::isCommandModeActive() const {
    if (!command_active) {
        return false;
    }
    
    // タイムアウトチェック
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - activation_time);
    
    return duration.count() < COMMAND_TIMEOUT_MS;
}

void CommandController::executeDirectionCommand(cv::Point2f direction) {
    if (!isCommandModeActive()) {
        return;
    }
    
    sendArrowKey(direction);
}

void CommandController::sendArrowKey(cv::Point2f direction) {
    // 最も強い方向成分を選択
    double abs_x = abs(direction.x);
    double abs_y = abs(direction.y);
    
    if (abs_x > abs_y) {
        // 左右の動き
        if (direction.x > 0) {
            std::cout << "→ Right" << std::endl;
#ifdef _WIN32
            sendWindowsKey(VK_RIGHT);
#elif __linux__
            sendLinuxKey(XK_Right);
#endif
        } else {
            std::cout << "← Left" << std::endl;
#ifdef _WIN32
            sendWindowsKey(VK_LEFT);
#elif __linux__
            sendLinuxKey(XK_Left);
#endif
        }
    } else {
        // 上下の動き
        if (direction.y > 0) {
            std::cout << "↓ Down" << std::endl;
#ifdef _WIN32
            sendWindowsKey(VK_DOWN);
#elif __linux__
            sendLinuxKey(XK_Down);
#endif
        } else {
            std::cout << "↑ Up" << std::endl;
#ifdef _WIN32
            sendWindowsKey(VK_UP);
#elif __linux__
            sendLinuxKey(XK_Up);
#endif
        }
    }
}

#ifdef _WIN32
void CommandController::sendWindowsKey(int vk_code) {
    keybd_event(vk_code, 0, 0, 0);
    Sleep(50);
    keybd_event(vk_code, 0, KEYEVENTF_KEYUP, 0);
}
#elif __linux__
void CommandController::sendLinuxKey(int key_code) {
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        KeyCode keycode = XKeysymToKeycode(display, key_code);
        XTestFakeKeyEvent(display, keycode, True, 0);
        XFlush(display);
        usleep(50000); // 50ms
        XTestFakeKeyEvent(display, keycode, False, 0);
        XFlush(display);
        XCloseDisplay(display);
    }
}
#endif
