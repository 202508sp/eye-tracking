#include "EyeTracker.h"
#include "Utils.h"
#include <iostream>

int main() {
    std::cout << "Eye Tracking System Starting..." << std::endl;
    
    // 設定ファイルの読み込み
    if (!Utils::loadConfig(Utils::getConfigPath())) {
        std::cout << "Warning: Could not load config file, using defaults" << std::endl;
    }
    
    // EyeTrackerの初期化
    EyeTracker tracker;
    if (!tracker.initialize(0)) {
        std::cerr << "Failed to initialize eye tracker" << std::endl;
        return -1;
    }
    
    std::cout << "Eye tracker initialized successfully" << std::endl;
    std::cout << "Double blink to activate command mode" << std::endl;
    std::cout << "Press ESC to exit" << std::endl;
    
    // メインループ実行
    tracker.run();
    
    std::cout << "Eye tracking system stopped" << std::endl;
    return 0;
}
