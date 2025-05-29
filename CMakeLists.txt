cmake_minimum_required(VERSION 3.10)
project(EyeTrackingSystem)

set(CMAKE_CXX_STANDARD 17)

# OpenCVを検索
find_package(OpenCV REQUIRED)

include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

set(SOURCES
    src/main.cpp
    src/EyeTracker.cpp
    src/BlinkDetector.cpp
    src/GazeEstimator.cpp
    src/CommandController.cpp
    src/Utils.cpp
)

# プラットフォーム固有のファイルを追加
if(WIN32)
    list(APPEND SOURCES src/platform/WindowsController.cpp)
endif()

add_executable(eye_tracker ${SOURCES})

target_link_libraries(eye_tracker ${OpenCV_LIBS})

if(WIN32)
    target_link_libraries(eye_tracker user32)
endif()


# リソースファイルのコピー
configure_file(${CMAKE_SOURCE_DIR}/config/config.xml 
               ${CMAKE_BINARY_DIR}/config.xml COPYONLY)
