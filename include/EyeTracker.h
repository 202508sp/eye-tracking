#ifndef EYETRACKER_H
#define EYETRACKER_H

#include <opencv2/opencv.hpp>
#include <memory>
#include "BlinkDetector.h"
#include "GazeEstimator.h"
#include "CommandController.h"

class EyeTracker {
private:
    cv::VideoCapture cap;
    std::unique_ptr<BlinkDetector> blink_detector;
    std::unique_ptr<GazeEstimator> gaze_estimator;
    std::unique_ptr<CommandController> command_controller;
    
    bool is_running;
    bool command_mode_active;
    cv::Mat current_frame;
    
public:
    EyeTracker();
    ~EyeTracker();
    
    bool initialize(int camera_id = 0);
    void run();
    void stop();
    
private:
    void processFrame();
    void handleDoubleBlinkDetected();
    void handleGazeDirection(cv::Point2f direction);
};

#endif
