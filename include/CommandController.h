#ifndef COMMANDCONTROLLER_H
#define COMMANDCONTROLLER_H

#include <opencv2/opencv.hpp>
#include <chrono>

class CommandController {
private:
    bool command_active;
    std::chrono::steady_clock::time_point activation_time;
    static const int COMMAND_TIMEOUT_MS = 5000;
    
public:
    CommandController();
    
    void activateCommandMode();
    void deactivateCommandMode();
    bool isCommandModeActive() const;
    
    void executeDirectionCommand(cv::Point2f direction);
    
private:
    void sendArrowKey(cv::Point2f direction);
    bool isTimeoutExpired();
    
#ifdef _WIN32
    void sendWindowsKey(int vk_code);
#elif __linux__
    void sendLinuxKey(int key_code);
#endif
};

#endif
