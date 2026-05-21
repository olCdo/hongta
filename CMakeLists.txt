cmake_minimum_required(VERSION 3.16)

project(honta_vision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio highgui)

find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h)
find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat)
find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec)
find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil)
find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale)

set(HONTA_HAS_FFMPEG OFF)
if(FFMPEG_INCLUDE_DIR
   AND FFMPEG_AVFORMAT_LIBRARY
   AND FFMPEG_AVCODEC_LIBRARY
   AND FFMPEG_AVUTIL_LIBRARY
   AND FFMPEG_SWSCALE_LIBRARY)
    set(HONTA_HAS_FFMPEG ON)
    set(HONTA_FFMPEG_LIBS
        ${FFMPEG_AVFORMAT_LIBRARY}
        ${FFMPEG_AVCODEC_LIBRARY}
        ${FFMPEG_AVUTIL_LIBRARY}
        ${FFMPEG_SWSCALE_LIBRARY}
    )
    message(STATUS "FFmpeg found: ${FFMPEG_INCLUDE_DIR}")
else()
    message(WARNING "FFmpeg development libraries were not found; phase2 RTSP pipeline targets will be skipped")
endif()

add_library(honta_vision
    code/src/vision/circle_detector.cpp
    code/src/vision/rtsp_reader.cpp
    code/src/vision/detection_session.cpp
)

if(HONTA_HAS_FFMPEG)
    target_sources(honta_vision
        PRIVATE
            code/src/vision/ffmpeg_rtsp_input.cpp
            code/src/vision/rtsp_output_service.cpp
            code/src/vision/detection_service.cpp
    )
endif()

target_include_directories(honta_vision
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
)

if(HONTA_HAS_FFMPEG)
    target_include_directories(honta_vision
        PRIVATE
            ${FFMPEG_INCLUDE_DIR}
    )
endif()

target_link_libraries(honta_vision
    PUBLIC
        ${OpenCV_LIBS}
)

if(HONTA_HAS_FFMPEG)
    target_link_libraries(honta_vision
        PRIVATE
            ${HONTA_FFMPEG_LIBS}
    )
endif()

add_executable(detect_stream_demo
    code/tools/detect_stream_demo.cpp
)

target_link_libraries(detect_stream_demo
    PRIVATE
        honta_vision
)

target_include_directories(detect_stream_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/code/third_party
)

if(HONTA_HAS_FFMPEG)
    add_executable(phase2_rtsp_pipeline_demo
        code/tools/phase2_rtsp_pipeline_demo.cpp
    )

    target_link_libraries(phase2_rtsp_pipeline_demo
        PRIVATE
            honta_vision
    )

    target_include_directories(phase2_rtsp_pipeline_demo
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/code/third_party
    )
endif()
