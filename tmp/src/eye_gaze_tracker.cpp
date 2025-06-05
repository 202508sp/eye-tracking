#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#elif __linux__
    #include <X11/Xlib.h>
    #include <X11/extensions/XTest.h>
    #include <X11/keysym.h>
    #include <unistd.h>
#endif

class EyeGazeTracker {
private:
    // OpenCV オブジェクト
    cv::VideoCapture cap;
    cv::CascadeClassifier face_cascade;
    cv::CascadeClassifier eye_cascade;

    // 視線追跡パラメータ
    cv::Point2f baseline_left_pupil;
    cv::Point2f baseline_right_pupil;
    std::vector<cv::Point2f> calibration_points;
    std::vector<cv::Point2f> gaze_points;
    
    bool is_calibrated;

    // 閾値設定
    const double HORIZONTAL_THRESHOLD = 15.0;
    const double VERTICAL_THRESHOLD = 8.0;
    const int MIN_ACTIVATION_FRAMES = 3;

    // フレーム追跡変数
    int up_count, down_count, left_count, right_count;
    std::chrono::steady_clock::time_point last_key_time;
    const std::chrono::milliseconds KEY_COOLDOWN{500};

#ifdef __linux__
    Display* display;
#endif

    /**
     * ユーザーからIPアドレスを取得
     */
    std::string getUserIPInput() {
        std::string ip;
        std::cout << "\n=== カメラIPアドレス入力 ===" << std::endl;
        std::cout << "IPアドレスを入力してください (例: 192.168.1.100)" << std::endl;
        std::cout << "何も入力せずEnterを押すとスキップ: ";
        
        std::getline(std::cin, ip);
        
        // 空白文字を削除
        ip.erase(0, ip.find_first_not_of(" \t"));
        ip.erase(ip.find_last_not_of(" \t") + 1);
        
        return ip;
    }

    /**
     * IPアドレスから様々なURL形式を生成
     */
    std::vector<std::string> generateIPCameraURLs(const std::string& ip) {
        std::vector<std::string> urls;
        
        if (ip.empty()) {
            return urls;
        }
        
        // 様々なIPカメラのURL形式を試行
        urls.push_back("http://" + ip + ":8080/video");              // IP Webcam
        urls.push_back("http://" + ip + ":4747/video");              // DroidCam
        urls.push_back("http://" + ip + ":8080/videofeed");          // 汎用MJPEG
        urls.push_back("http://" + ip + ":8080/shot.jpg");           // 静止画
        urls.push_back("http://" + ip + ":8080/mjpg/video.mjpg");    // MJPEG ストリーム
        urls.push_back("rtsp://" + ip + ":554/stream");              // RTSP
        urls.push_back("rtsp://" + ip + ":8554/");                   // RTSP 代替
        urls.push_back("http://" + ip + "/video");                   // シンプル形式
        urls.push_back("http://" + ip + ":81/videostream.cgi");      // 一部のIPカメラ
        urls.push_back("http://" + ip + ":8000/video");              // カスタムポート
        
        return urls;
    }

    /**
     * 単一のカメラソースをテスト
     */
    bool testCameraSource(const std::string& source) {
        std::cout << "テスト中: " << source << std::endl;
        
        cv::VideoCapture test_cap;
        
        try {
            if (source == "0" || source == "1" || source == "2" || source == "3") {
                // ローカルカメラ
                int camera_id = std::stoi(source);
                test_cap.open(camera_id, cv::CAP_DSHOW);
            } else {
                // IPカメラ/ネットワークストリーム
                test_cap.open(source);
            }
            
            if (test_cap.isOpened()) {
                // タイムアウト付きフレーム取得テスト
                cv::Mat test_frame;
                auto start_time = std::chrono::steady_clock::now();
                
                while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
                    test_cap >> test_frame;
                    if (!test_frame.empty()) {
                        std::cout << "✓ 接続成功: " << source << std::endl;
                        std::cout << "  解像度: " << test_frame.cols << "x" << test_frame.rows << std::endl;
                        
                        // 成功したら実際のキャプチャオブジェクトに移す
                        cap = std::move(test_cap);
                        return true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                std::cout << "✗ タイムアウト: " << source << std::endl;
                test_cap.release();
            } else {
                std::cout << "✗ オープン失敗: " << source << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "✗ エラー: " << source << " - " << e.what() << std::endl;
        }
        
        return false;
    }

    /**
     * 対話式カメラ初期化
     */
    bool initializeCameraInteractive() {
        // 最初にローカルカメラを試行
        std::cout << "=== カメラ初期化 ===" << std::endl;
        std::cout << "ローカルカメラを検索中..." << std::endl;
        
        std::vector<std::string> local_cameras = {"0", "1", "2"};
        for (const auto& camera : local_cameras) {
            if (testCameraSource(camera)) {
                return true;
            }
        }
        
        std::cout << "ローカルカメラが見つかりませんでした。" << std::endl;
        
        // IPカメラの対話式設定
        while (true) {
            std::string ip = getUserIPInput();
            
            if (ip.empty()) {
                std::cout << "IPアドレスが入力されませんでした。" << std::endl;
                
                char choice;
                std::cout << "再試行しますか？ (y/n): ";
                std::cin >> choice;
                std::cin.ignore(); // 改行文字をクリア
                
                if (choice != 'y' && choice != 'Y') {
                    return false;
                }
                continue;
            }
            
            std::cout << "IP " << ip << " のカメラURLを試行中..." << std::endl;
            
            auto urls = generateIPCameraURLs(ip);
            bool success = false;
            
            for (const auto& url : urls) {
                if (testCameraSource(url)) {
                    success = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            if (success) {
                return true;
            }
            
            std::cout << "IP " << ip << " では接続できませんでした。" << std::endl;
            
            char choice;
            std::cout << "別のIPアドレスを試しますか？ (y/n): ";
            std::cin >> choice;
            std::cin.ignore(); // 改行文字をクリア
            
            if (choice != 'y' && choice != 'Y') {
                return false;
            }
        }
    }

    /**
     * カメラ設定の最適化
     */
    void optimizeCameraSettings() {
        if (!cap.isOpened()) return;
        
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        
        std::cout << "\nカメラ設定:" << std::endl;
        std::cout << "  幅: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << std::endl;
        std::cout << "  高さ: " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
        std::cout << "  FPS: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
    }

public:
    /**
     * コンストラクタ
     */
    EyeGazeTracker() : is_calibrated(false), up_count(0), down_count(0),
                       left_count(0), right_count(0) {
        
        // 対話式カメラ初期化
        if (!initializeCameraInteractive()) {
            throw std::runtime_error("カメラの初期化に失敗しました");
        }
        
        // カメラ設定の最適化
        optimizeCameraSettings();

        // OpenCV カスケード分類器の初期化
        if (!face_cascade.load("haarcascade_frontalface_alt.xml")) {
            throw std::runtime_error("顔検出カスケードファイルを読み込めませんでした");
        }
        if (!eye_cascade.load("haarcascade_eye.xml")) {
            throw std::runtime_error("目検出カスケードファイルを読み込めませんでした");
        }

#ifdef __linux__
        display = XOpenDisplay(nullptr);
        if (!display) {
            throw std::runtime_error("X11ディスプレイを開けませんでした");
        }
#endif

        last_key_time = std::chrono::steady_clock::now();
        std::cout << "\n視線追跡システムが初期化されました" << std::endl;
        std::cout << "キャリブレーション中... 正面を見つめて'c'キーを押してください" << std::endl;
    }

    ~EyeGazeTracker() {
#ifdef __linux__
        if (display) {
            XCloseDisplay(display);
        }
#endif
    }

    /**
     * 実行中にカメラを再設定
     */
    void reconfigureCamera() {
        if (cap.isOpened()) {
            cap.release();
        }
        
        std::cout << "\n=== カメラ再設定 ===" << std::endl;
        
        if (initializeCameraInteractive()) {
            optimizeCameraSettings();
            std::cout << "カメラが正常に再設定されました。" << std::endl;
        } else {
            std::cout << "カメラの再設定に失敗しました。" << std::endl;
        }
    }

    // ... 既存のメソッド（detectPupilCenter, extractEyeRegions, calibrate, sendKeyInput, processGazeDirection）...
    // [前回のコードと同じなので省略]

    /**
     * 目の領域から瞳孔中心を検出
     */
    cv::Point2f detectPupilCenter(const cv::Mat& eye_region) {
        cv::Mat gray_eye, binary_eye;

        if (eye_region.channels() == 3) {
            cv::cvtColor(eye_region, gray_eye, cv::COLOR_BGR2GRAY);
        } else {
            gray_eye = eye_region.clone();
        }

        cv::GaussianBlur(gray_eye, gray_eye, cv::Size(5, 5), 0);

        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray_eye, circles, cv::HOUGH_GRADIENT, 1,
                         gray_eye.rows / 8, 100, 30, 
                         gray_eye.rows / 8, gray_eye.rows / 3);

        if (!circles.empty()) {
            cv::Vec3f largest_circle = circles[0];
            for (const auto& circle : circles) {
                if (circle[2] > largest_circle[2]) {
                    largest_circle = circle;
                }
            }
            return cv::Point2f(largest_circle[0], largest_circle[1]);
        }

        cv::threshold(gray_eye, binary_eye, 50, 255, cv::THRESH_BINARY_INV);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary_eye, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (contours.empty()) {
            return cv::Point2f(-1, -1);
        }

        double max_area = 0;
        int max_contour_idx = 0;

        for (size_t i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            if (area > max_area && area > 30) {
                max_area = area;
                max_contour_idx = i;
            }
        }

        if (max_area > 30) {
            cv::Moments moments = cv::moments(contours[max_contour_idx]);
            if (moments.m00 != 0) {
                cv::Point2f center(moments.m10 / moments.m00, moments.m01 / moments.m00);
                return center;
            }
        }

        return cv::Point2f(-1, -1);
    }

    std::vector<cv::Rect> extractEyeRegions(const cv::Mat& face_roi, const cv::Rect& face_rect) {
        std::vector<cv::Rect> eyes;
        
        cv::Rect eye_search_area(0, 0, face_roi.cols, face_roi.rows / 2);
        cv::Mat eye_search_roi = face_roi(eye_search_area);
        
        std::vector<cv::Rect> detected_eyes;
        eye_cascade.detectMultiScale(eye_search_roi, detected_eyes, 1.1, 3, 0, cv::Size(15, 15));
        
        for (auto& eye : detected_eyes) {
            eye.x += face_rect.x + eye_search_area.x;
            eye.y += face_rect.y + eye_search_area.y;
            eyes.push_back(eye);
        }
        
        if (eyes.size() >= 2) {
            std::sort(eyes.begin(), eyes.end(), 
                [](const cv::Rect& a, const cv::Rect& b) {
                    return a.x < b.x;
                });
        }
        
        return eyes;
    }

    void calibrate(const cv::Point2f& left_pupil, const cv::Point2f& right_pupil) {
        baseline_left_pupil = left_pupil;
        baseline_right_pupil = right_pupil;
        is_calibrated = true;
        std::cout << "キャリブレーション完了!" << std::endl;
        std::cout << "左目基準点: (" << left_pupil.x << ", " << left_pupil.y << ")" << std::endl;
        std::cout << "右目基準点: (" << right_pupil.x << ", " << right_pupil.y << ")" << std::endl;
    }

    void sendKeyInput(const std::string& direction) {
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - last_key_time < KEY_COOLDOWN) {
            return;
        }

#ifdef _WIN32
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.dwFlags = 0;

        if (direction == "up") {
            input.ki.wVk = VK_UP;
        } else if (direction == "down") {
            input.ki.wVk = VK_DOWN;
        } else if (direction == "left") {
            input.ki.wVk = VK_LEFT;
        } else if (direction == "right") {
            input.ki.wVk = VK_RIGHT;
        }

        SendInput(1, &input, sizeof(INPUT));
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));

#elif __linux__
        if (!display) return;

        KeySym key_sym;
        if (direction == "up") {
            key_sym = XK_Up;
        } else if (direction == "down") {
            key_sym = XK_Down;
        } else if (direction == "left") {
            key_sym = XK_Left;
        } else if (direction == "right") {
            key_sym = XK_Right;
        } else {
            return;
        }

        KeyCode key_code = XKeysymToKeycode(display, key_sym);
        XTestFakeKeyEvent(display, key_code, True, 0);
        XTestFakeKeyEvent(display, key_code, False, 0);
        XFlush(display);
#endif

        last_key_time = current_time;
        std::cout << "キー入力送信: " << direction << std::endl;
    }

    void processGazeDirection(const cv::Point2f& current_left_pupil, 
                             const cv::Point2f& current_right_pupil) {
        if (!is_calibrated || current_left_pupil.x < 0 || current_right_pupil.x < 0) {
            return;
        }

        float left_dx = current_left_pupil.x - baseline_left_pupil.x;
        float left_dy = current_left_pupil.y - baseline_left_pupil.y;
        float right_dx = current_right_pupil.x - baseline_right_pupil.x;
        float right_dy = current_right_pupil.y - baseline_right_pupil.y;

        float avg_dx = (left_dx + right_dx) / 2.0f;
        float avg_dy = (left_dy + right_dy) / 2.0f;

        bool moved_up = avg_dy < -VERTICAL_THRESHOLD;
        bool moved_down = avg_dy > VERTICAL_THRESHOLD;
        bool moved_left = avg_dx < -HORIZONTAL_THRESHOLD;
        bool moved_right = avg_dx > HORIZONTAL_THRESHOLD;

        up_count = moved_up ? up_count + 1 : 0;
        down_count = moved_down ? down_count + 1 : 0;
        left_count = moved_left ? left_count + 1 : 0;
        right_count = moved_right ? right_count + 1 : 0;

        if (up_count >= MIN_ACTIVATION_FRAMES) {
            sendKeyInput("up");
            up_count = 0;
        } else if (down_count >= MIN_ACTIVATION_FRAMES) {
            sendKeyInput("down");
            down_count = 0;
        } else if (left_count >= MIN_ACTIVATION_FRAMES) {
            sendKeyInput("left");
            left_count = 0;
        } else if (right_count >= MIN_ACTIVATION_FRAMES) {
            sendKeyInput("right");
            right_count = 0;
        }
    }

    /**
     * メインループ実行
     */
    void run() {
        cv::Mat frame, gray;

        while (true) {
            auto frame_start = std::chrono::high_resolution_clock::now();

            cap >> frame;
            if (frame.empty()) {
                std::cerr << "フレーム取得エラー - カメラ再設定を試みます..." << std::endl;
                reconfigureCamera();
                continue;
            }

            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

            std::vector<cv::Rect> faces;
            face_cascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(30, 30));

            if (!faces.empty()) {
                cv::Rect face_rect = faces[0];
                cv::rectangle(frame, face_rect, cv::Scalar(255, 0, 0), 2);

                cv::Mat face_roi = gray(face_rect);
                std::vector<cv::Rect> eye_rects = extractEyeRegions(face_roi, face_rect);

                if (eye_rects.size() >= 2) {
                    cv::Rect left_eye_rect = eye_rects[0];
                    cv::Rect right_eye_rect = eye_rects[1];

                    left_eye_rect.x = std::max(0, left_eye_rect.x - 10);
                    left_eye_rect.y = std::max(0, left_eye_rect.y - 5);
                    left_eye_rect.width = std::min(left_eye_rect.width + 20, frame.cols - left_eye_rect.x);
                    left_eye_rect.height = std::min(left_eye_rect.height + 10, frame.rows - left_eye_rect.y);

                    right_eye_rect.x = std::max(0, right_eye_rect.x - 10);
                    right_eye_rect.y = std::max(0, right_eye_rect.y - 5);
                    right_eye_rect.width = std::min(right_eye_rect.width + 20, frame.cols - right_eye_rect.x);
                    right_eye_rect.height = std::min(right_eye_rect.height + 10, frame.rows - right_eye_rect.y);

                    cv::Mat left_eye_roi = frame(left_eye_rect);
                    cv::Mat right_eye_roi = frame(right_eye_rect);

                    cv::Point2f left_pupil = detectPupilCenter(left_eye_roi);
                    cv::Point2f right_pupil = detectPupilCenter(right_eye_roi);

                    if (left_pupil.x >= 0 && right_pupil.x >= 0) {
                        left_pupil.x += left_eye_rect.x;
                        left_pupil.y += left_eye_rect.y;
                        right_pupil.x += right_eye_rect.x;
                        right_pupil.y += right_eye_rect.y;

                        if (is_calibrated) {
                            processGazeDirection(left_pupil, right_pupil);
                        }

                        cv::circle(frame, left_pupil, 3, cv::Scalar(0, 255, 0), -1);
                        cv::circle(frame, right_pupil, 3, cv::Scalar(0, 255, 0), -1);
                    }

                    cv::rectangle(frame, left_eye_rect, cv::Scalar(255, 0, 0), 1);
                    cv::rectangle(frame, right_eye_rect, cv::Scalar(255, 0, 0), 1);
                }
            }

            std::string status = is_calibrated ? "Calibrated" : "Press 'c' to Calibrate";
            cv::putText(frame, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 
                       0.8, cv::Scalar(255, 255, 255), 2);

            auto frame_end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
            double fps = 1000.0 / duration.count();

            cv::putText(frame, "FPS: " + std::to_string((int)fps), cv::Point(10, 60), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

            // 操作説明を追加
            cv::putText(frame, "Press 'r' to reconfigure camera", cv::Point(10, 90), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

            cv::imshow("Eye Gaze Tracker", frame);

            char key = cv::waitKey(1) & 0xFF;
            if (key == 'q') {
                break;
            } else if (key == 'r') {
                // カメラ再設定
                reconfigureCamera();
            } else if (key == 'c' && !faces.empty()) {
                // キャリブレーション実行
                cv::Rect face_rect = faces[0];
                cv::Mat face_roi = gray(face_rect);
                std::vector<cv::Rect> eye_rects = extractEyeRegions(face_roi, face_rect);

                if (eye_rects.size() >= 2) {
                    cv::Rect left_eye_rect = eye_rects[0];
                    cv::Rect right_eye_rect = eye_rects[1];

                    cv::Mat left_eye_roi = frame(left_eye_rect);
                    cv::Mat right_eye_roi = frame(right_eye_rect);

                    cv::Point2f left_pupil = detectPupilCenter(left_eye_roi);
                    cv::Point2f right_pupil = detectPupilCenter(right_eye_roi);

                    if (left_pupil.x >= 0 && right_pupil.x >= 0) {
                        left_pupil.x += left_eye_rect.x;
                        left_pupil.y += left_eye_rect.y;
                        right_pupil.x += right_eye_rect.x;
                        right_pupil.y += right_eye_rect.y;

                        calibrate(left_pupil, right_pupil);
                    }
                }
            }
        }

        cap.release();
        cv::destroyAllWindows();
    }
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        std::cout << "===== 視線追跡キー入力エミュレーションシステム =====" << std::endl;
        std::cout << "使用方法:" << std::endl;
        std::cout << "1. 'c'キー: キャリブレーション実行" << std::endl;
        std::cout << "2. 'r'キー: カメラ再設定" << std::endl;
        std::cout << "3. 'q'キー: プログラム終了" << std::endl;
        std::cout << "4. 視線移動: 矢印キー入力送信" << std::endl;
        std::cout << "=========================================" << std::endl;

        EyeGazeTracker tracker;
        tracker.run();

    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        std::cout << "何かキーを押してください..." << std::endl;
        std::cin.get();
        return -1;
    }

    return 0;
}
