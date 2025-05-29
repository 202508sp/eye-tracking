#include "BlinkDetector.h"
#include <algorithm>
#include <iostream>

BlinkDetector::BlinkDetector(double threshold, int frames) 
    : ear_threshold(threshold), consecutive_frames(frames), 
      frame_counter(0), is_blinking(false) {
}

bool BlinkDetector::detectBlink(const cv::Mat& eye_roi) {
    double ear = calculateEAR(eye_roi);
    
    if (ear < ear_threshold) {
        frame_counter++;
        if (frame_counter >= consecutive_frames && !is_blinking) {
            is_blinking = true;
            auto now = std::chrono::steady_clock::now();
            blink_times.push_back(now);
            last_blink_time = now;
            return true;
        }
    } else {
        if (is_blinking) {
            is_blinking = false;
        }
        frame_counter = 0;
    }
    
    return false;
}

double BlinkDetector::calculateEAR(const cv::Mat& eye_roi) {
    std::vector<cv::Point> eye_contour = extractEyeContour(eye_roi);
    
    if (eye_contour.size() < 6) {
        return 1.0; // デフォルト値（目が開いている状態）
    }
    
    // 簡単なEAR計算（実際の実装では68点ランドマークを使用）
    // ここでは目の輪郭から近似的に計算
    double vertical_1 = euclideanDistance(eye_contour[1], eye_contour[5]);
    double vertical_2 = euclideanDistance(eye_contour[2], eye_contour[4]);
    double horizontal = euclideanDistance(eye_contour[0], eye_contour[3]);
    
    double ear = (vertical_1 + vertical_2) / (2.0 * horizontal);
    return ear;
}

bool BlinkDetector::checkDoubleBlinkPattern() {
    if (blink_times.size() < 2) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto recent_blinks = blink_times;
    
    // 古い瞬きデータを削除（5秒以上前）
    recent_blinks.erase(
        std::remove_if(recent_blinks.begin(), recent_blinks.end(),
        [now](const std::chrono::steady_clock::time_point& blink_time) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - blink_time);
            return duration.count() > 5000;
        }),
        recent_blinks.end()
    );
    
    if (recent_blinks.size() >= 2) {
        auto last_two = recent_blinks.end() - 2;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            recent_blinks.back() - *last_two);
        
        if (duration.count() >= MIN_BLINK_INTERVAL_MS && 
            duration.count() <= MAX_BLINK_INTERVAL_MS) {
            blink_times.clear(); // パターン検出後にリセット
            return true;
        }
    }
    
    return false;
}

std::vector<cv::Point> BlinkDetector::extractEyeContour(const cv::Mat& eye_roi) {
    cv::Mat gray, binary;
    cv::cvtColor(eye_roi, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return std::vector<cv::Point>();
    }
    
    // 最大の輪郭を目の輪郭として選択
    auto max_contour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });
    
    return max_contour;
}

double BlinkDetector::euclideanDistance(const cv::Point& p1, const cv::Point& p2) {
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

void BlinkDetector::reset() {
    blink_times.clear();
    frame_counter = 0;
    is_blinking = false;
}
