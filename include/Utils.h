#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <opencv2/opencv.hpp>

class Utils {
public:
    static std::string getConfigPath();
    static std::string getDataPath();
    static bool loadConfig(const std::string& config_path);
    
    static void saveCalibrationData(const std::string& filename, 
                                   const cv::Point2f& baseline);
    static cv::Point2f loadCalibrationData(const std::string& filename);
    
    static void drawDebugInfo(cv::Mat& frame, 
                             const cv::Point2f& pupil_pos,
                             const cv::Point2f& gaze_direction,
                             bool command_active);
    
    static double calculateFPS();
    
private:
    static std::chrono::steady_clock::time_point last_time;
    static int frame_count;
};

#endif
