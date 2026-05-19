cmake_minimum_required(VERSION 3.16)

project(honta_vision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio highgui)

add_library(honta_vision
    src/vision/circle_detector.cpp
    src/vision/rtsp_reader.cpp
)

target_include_directories(honta_vision
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(honta_vision
    PUBLIC
        ${OpenCV_LIBS}
)

add_executable(detect_demo
    tools/detect_demo.cpp
)

target_link_libraries(detect_demo
    PRIVATE
        honta_vision
)

add_executable(detect_stream_demo
    tools/detect_stream_demo.cpp
)

target_link_libraries(detect_stream_demo
    PRIVATE
        honta_vision
)

target_include_directories(detect_stream_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
