#include "GazeEstimator.h"
#include <iostream>

GazeEstimator::GazeEstimator(double threshold, double deadzone) 
    : movement_threshold(threshold), deadzone_radius(deadzone), 
      is_calibrated(false) {
}

cv::Point2f GazeEstimator::calculateGazeDirection(const cv::Mat& eye_roi) {
    if (!is_calibrated) {
        return cv::Point2f(0, 0);
    }
    
    cv::Point2f current_pupil = detectPupilCenter(eye_roi);
    if (current_pupil.x < 0 || current_pupil.y < 0) {
        return cv::Point2f(0, 0);
    }
    
    // 正規化された相対位置を計算
    cv::Point2f relative_pos;
    relative_pos.x = (current_pupil.x - baseline_pupil_pos.x) / (eye_roi_size.width * 0.5);
    relative_pos.y = (current_pupil.y - baseline_pupil_pos.y) / (eye_roi_size.height * 0.5);
    
    // デッドゾーンの適用
    double magnitude = sqrt(relative_pos.x * relative_pos.x + relative_pos.y * relative_pos.y);
    if (magnitude < deadzone_radius) {
        return cv::Point2f(0, 0);
    }
    
    // 移動閾値の適用
    if (magnitude < movement_threshold) {
        return cv::Point2f(0, 0);
    }
    
    return relative_pos;
}

cv::Point2f GazeEstimator::detectPupilCenter(const cv::Mat& eye_roi) {
    cv::Point2f pupil_center = findPupilUsingHoughCircles(eye_roi);
    
    if (pupil_center.x < 0 || pupil_center.y < 0) {
        pupil_center = findPupilUsingContours(eye_roi);
    }
    
    return pupil_center;
}

void GazeEstimator::calibrateBaseline(const cv::Mat& eye_roi) {
    baseline_pupil_pos = detectPupilCenter(eye_roi);
    eye_roi_size = eye_roi.size();
    
    if (baseline_pupil_pos.x >= 0 && baseline_pupil_pos.y >= 0) {
        is_calibrated = true;
        std::cout << "Baseline calibrated: " << baseline_pupil_pos << std::endl;
    }
}

cv::Point2f GazeEstimator::findPupilUsingHoughCircles(const cv::Mat& eye_roi) {
    cv::Mat processed = preprocessEyeImage(eye_roi);
    
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(processed, circles, cv::HOUGH_GRADIENT, 1,
                     processed.rows / 8, 100, 30, 
                     processed.rows / 8, processed.rows / 3);
    
    if (!circles.empty()) {
        cv::Vec3f largest_circle = circles[0];
        for (const auto& circle : circles) {
            if (circle[2] > largest_circle[2]) {
                largest_circle = circle;
            }
        }
        return cv::Point2f(largest_circle[0], largest_circle[1]);
    }
    
    return cv::Point2f(-1, -1);
}

cv::Point2f GazeEstimator::findPupilUsingContours(const cv::Mat& eye_roi) {
    cv::Mat processed = preprocessEyeImage(eye_roi);
    
    // 反転して瞳孔を白にする
    cv::Mat inverted;
    cv::bitwise_not(processed, inverted);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(inverted, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return cv::Point2f(-1, -1);
    }
    
    // 最大面積の輪郭を瞳孔として選択
    auto max_contour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });
    
    cv::Moments moments = cv::moments(max_contour);
    if (moments.m00 != 0) {
        cv::Point2f centroid(moments.m10 / moments.m00, moments.m01 / moments.m00);
        return centroid;
    }
    
    return cv::Point2f(-1, -1);
}

cv::Mat GazeEstimator::preprocessEyeImage(const cv::Mat& eye_roi) {
    cv::Mat gray, processed;
    
    if (eye_roi.channels() == 3) {
        cv::cvtColor(eye_roi, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = eye_roi.clone();
    }
    
    // ガウシアンブラーでノイズ除去
    cv::GaussianBlur(gray, processed, cv::Size(5, 5), 0);
    
    // 適応的閾値処理
    cv::adaptiveThreshold(processed, processed, 255, 
                         cv::ADAPTIVE_THRESH_MEAN_C, 
                         cv::THRESH_BINARY, 11, 2);
    
    return processed;
}
