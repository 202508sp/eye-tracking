cmake_minimum_required(VERSION 3.12)
project(EyeGazeTracker VERSION 1.0.0 LANGUAGES CXX)

# C++17設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OpenCVでQt GUIを無効化
set(WITH_QT OFF)
set(OpenCV_STATIC ON)

# OpenCV検索（highguiを追加）
find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect videoio imgcodecs highgui)

# 実行ファイル作成
add_executable(EyeGazeTracker src/eye_gaze_tracker.cpp)

# ライブラリリンク
target_link_libraries(EyeGazeTracker ${OpenCV_LIBS})

# プラットフォーム固有の設定
if(WIN32)
    # Windows用設定
    target_link_libraries(EyeGazeTracker user32)
    target_compile_definitions(EyeGazeTracker PRIVATE 
        WIN32_LEAN_AND_MEAN 
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        # Qt GUI無効化
        NO_QT
        OPENCV_DISABLE_EIGEN_TENSOR_SUPPORT
    )
    
    # コンパイラ別の設定
    if(MSVC)
        target_compile_options(EyeGazeTracker PRIVATE /W3 /MP)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(EyeGazeTracker PRIVATE 
            -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
        )
    endif()
    
elseif(UNIX AND NOT APPLE)
    # Linux用設定
    find_package(X11)
    find_library(XTEST_LIB Xtst)
    
    if(X11_FOUND AND XTEST_LIB)
        target_link_libraries(EyeGazeTracker 
            ${X11_LIBRARIES} 
            ${XTEST_LIB}
        )
        message(STATUS "X11 and XTest libraries found and linked")
    else()
        message(WARNING "X11 or XTest libraries not found - keyboard simulation may not work")
    endif()
    
    target_link_libraries(EyeGazeTracker pthread)
endif()

# ビルド情報表示
message(STATUS "=== Build Configuration ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "===========================")
