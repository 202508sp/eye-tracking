#include "EyeTracker.h"
#include "Utils.h"
#include <iostream>

EyeTracker::EyeTracker() 
    : is_running(false), command_mode_active(false) {
    blink_detector = std::make_unique<BlinkDetector>();
    gaze_estimator = std::make_unique<GazeEstimator>();
    command_controller = std::make_unique<CommandController>();
}

EyeTracker::~EyeTracker() {
    stop();
}

bool EyeTracker::initialize(int camera_id) {
    cap.open(camera_id);
    if (!cap.isOpened()) {
        std::cerr << "Failed to open camera " << camera_id << std::endl;
        return false;
    }
    
    // カメラ設定
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 30);
    
    return true;
}

void EyeTracker::run() {
    is_running = true;
    
    while (is_running) {
        cap >> current_frame;
        if (current_frame.empty()) {
            std::cerr << "Failed to capture frame" << std::endl;
            break;
        }
        
        processFrame();
        
        // ESCキーで終了
        char key = cv::waitKey(1) & 0xFF;
        if (key == 27) { // ESC key
            break;
        }
    }
    
    stop();
}

void EyeTracker::stop() {
    is_running = false;
    if (cap.isOpened()) {
        cap.release();
    }
    cv::destroyAllWindows();
}

void EyeTracker::processFrame() {
    // 目の付近映像のみなので、フレーム全体を目領域として処理
    cv::Mat eye_roi = current_frame.clone();
    
    // ダブル瞬き検出
    if (blink_detector->detectBlink(eye_roi)) {
        if (blink_detector->checkDoubleBlinkPattern()) {
            handleDoubleBlinkDetected();
        }
    }
    
    // コマンドモードがアクティブな場合、視線方向を検出
    if (command_mode_active) {
        if (!gaze_estimator->isCalibrated()) {
            gaze_estimator->calibrateBaseline(eye_roi);
        } else {
            cv::Point2f gaze_direction = gaze_estimator->calculateGazeDirection(eye_roi);
            handleGazeDirection(gaze_direction);
        }
    }
    
    // デバッグ情報の描画
    cv::Point2f pupil_pos = gaze_estimator->detectPupilCenter(eye_roi);
    cv::Point2f gaze_dir = gaze_estimator->calculateGazeDirection(eye_roi);
    Utils::drawDebugInfo(current_frame, pupil_pos, gaze_dir, command_mode_active);
    
    cv::imshow("Eye Tracking", current_frame);
}

void EyeTracker::handleDoubleBlinkDetected() {
    std::cout << "Double blink detected!" << std::endl;
    
    if (!command_mode_active) {
        command_controller->activateCommandMode();
        command_mode_active = true;
        std::cout << "Command mode activated" << std::endl;
    } else {
        command_controller->deactivateCommandMode();
        command_mode_active = false;
        std::cout << "Command mode deactivated" << std::endl;
    }
}

void EyeTracker::handleGazeDirection(cv::Point2f direction) {
    // 方向の大きさが閾値を超えた場合のみコマンドを実行
    double magnitude = sqrt(direction.x * direction.x + direction.y * direction.y);
    if (magnitude > 0.3) {
        command_controller->executeDirectionCommand(direction);
    }
    
    // コマンドモードのタイムアウトチェック
    if (!command_controller->isCommandModeActive()) {
        command_mode_active = false;
    }
}
