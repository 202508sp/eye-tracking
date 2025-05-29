#ifndef PLATFORMCONTROLLER_H
#define PLATFORMCONTROLLER_H

class PlatformController {
public:
    static void sendKeyPress(int key_code);
    static void moveMouse(int x, int y);
};

#endif
