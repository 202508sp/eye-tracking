#include "Utils.h"
#include <iostream>
#include <fstream>
#include <chrono>

std::chrono::steady_clock::time_point Utils::last_time = std::chrono::steady_clock::now();
int Utils::frame_count = 0;

std::string Utils::getConfigPath() {
#ifdef _WIN32
    char* appdata = getenv("LOCALAPPDATA");
    if (appdata) {
        return std::string(appdata) + "\\EyeTracker\\config.xml";
    }
    return ".\\config.xml";
#else
    char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.eyetracker/config.xml";
    }
    return "./config.xml";
#endif
}

std::string Utils::getDataPath() {
#ifdef _WIN32
    return ".\\data\\";
#else
    return "./data/";
#endif
}

bool Utils::loadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (file.is_open()) {
        std::cout << "Config loaded from: " << config_path << std::endl;
        file.close();
        return true;
    }
    return false;
}

void Utils::saveCalibrationData(const std::string& filename, 
                               const cv::Point2f& baseline) {
    cv::FileStorage fs(filename, cv::FileStorage::WRITE);
    if (fs.isOpened()) {
        fs << "baseline_x" << baseline.x;
        fs << "baseline_y" << baseline.y;
        fs << "timestamp" << static_cast<int>(std::time(nullptr));
        fs.release();
    }
}

cv::Point2f Utils::loadCalibrationData(const std::string& filename) {
    cv::FileStorage fs(filename, cv::FileStorage::READ);
    cv::Point2f baseline(-1, -1);
    
    if (fs.isOpened()) {
        fs["baseline_x"] >> baseline.x;
        fs["baseline_y"] >> baseline.y;
        fs.release();
    }
    
    return baseline;
}

void Utils::drawDebugInfo(cv::Mat& frame, 
                         const cv::Point2f& pupil_pos,
                         const cv::Point2f& gaze_direction,
                         bool command_active) {
    // 瞳孔位置を描画
    if (pupil_pos.x >= 0 && pupil_pos.y >= 0) {
        cv::circle(frame, pupil_pos, 3, cv::Scalar(0, 255, 0), -1);
    }
    
    // 視線方向を矢印で描画
    if (gaze_direction.x != 0 || gaze_direction.y != 0) {
        cv::Point2f center(frame.cols / 2, frame.rows / 2);
        cv::Point2f end_point = center + gaze_direction * 50;
        cv::arrowedLine(frame, center, end_point, cv::Scalar(255, 0, 0), 2);
    }
    
    // コマンドモード状態を表示
    std::string mode_text = command_active ? "COMMAND ACTIVE" : "MONITORING";
    cv::Scalar text_color = command_active ? cv::Scalar(0, 0, 255) : cv::Scalar(255, 255, 255);
    cv::putText(frame, mode_text, cv::Point(10, 30), 
                cv::FONT_HERSHEY_SIMPLEX, 1, text_color, 2);
    
    // FPS表示
    double fps = calculateFPS();
    cv::putText(frame, "FPS: " + std::to_string(static_cast<int>(fps)), 
                cv::Point(10, frame.rows - 10), 
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
}

double Utils::calculateFPS() {
    frame_count++;
    auto current_time = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - last_time);
    
    if (duration.count() >= 1000) { // 1秒経過
        double fps = frame_count * 1000.0 / duration.count();
        frame_count = 0;
        last_time = current_time;
        return fps;
    }
    
    return 0.0;
}
