#ifndef BLINKDETECTOR_H
#define BLINKDETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <chrono>

class BlinkDetector {
private:
    double ear_threshold;
    int consecutive_frames;
    int frame_counter;
    bool is_blinking;
    
    std::vector<std::chrono::steady_clock::time_point> blink_times;
    std::chrono::steady_clock::time_point last_blink_time;
    
    static const int MAX_BLINK_INTERVAL_MS = 800;
    static const int MIN_BLINK_INTERVAL_MS = 100;
    
public:
    BlinkDetector(double threshold = 0.25, int frames = 3);
    
    double calculateEAR(const cv::Mat& eye_roi);
    bool detectBlink(const cv::Mat& eye_roi);
    bool checkDoubleBlinkPattern();
    void reset();
    
private:
    std::vector<cv::Point> extractEyeContour(const cv::Mat& eye_roi);
    double euclideanDistance(const cv::Point& p1, const cv::Point& p2);
};

#endif
