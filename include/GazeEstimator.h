#ifndef GAZEESTIMATOR_H
#define GAZEESTIMATOR_H

#include <opencv2/opencv.hpp>

class GazeEstimator {
private:
    cv::Point2f baseline_pupil_pos;
    cv::Size eye_roi_size;
    bool is_calibrated;
    
    double movement_threshold;
    double deadzone_radius;
    
public:
    GazeEstimator(double threshold = 0.05, double deadzone = 0.1);
    
    cv::Point2f detectPupilCenter(const cv::Mat& eye_roi);
    cv::Point2f calculateGazeDirection(const cv::Mat& eye_roi);
    void calibrateBaseline(const cv::Mat& eye_roi);
    bool isCalibrated() const { return is_calibrated; }
    
private:
    cv::Point2f findPupilUsingHoughCircles(const cv::Mat& eye_roi);
    cv::Point2f findPupilUsingContours(const cv::Mat& eye_roi);
    cv::Mat preprocessEyeImage(const cv::Mat& eye_roi);
};

#endif
