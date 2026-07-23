cmake_minimum_required(VERSION 3.16)

project(honta_vision LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HONTA_ENABLE_MOCK_DETECTION "Enable deterministic mock detection for local smoke checks" OFF)

include(CTest)

add_library(honta_logging
    code/src/logging/native_log.cpp
)

target_include_directories(honta_logging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
)

if(ANDROID)
    find_library(ANDROID_LOG_LIBRARY log REQUIRED)
    target_link_libraries(honta_logging
        PUBLIC
            ${ANDROID_LOG_LIBRARY}
    )
endif()

if(ANDROID AND DEFINED VCPKG_INSTALLED_DIR AND NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencv4")
endif()

if(ANDROID)
    find_package(OpenCV QUIET COMPONENTS opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio opencv_calib3d)
else()
    find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs videoio highgui calib3d)
endif()

set(HONTA_HAS_OPENCV OFF)
if(OpenCV_FOUND)
    set(HONTA_HAS_OPENCV ON)
    message(STATUS "OpenCV found: ${OpenCV_DIR}")
else()
    message(WARNING "OpenCV was not found; vision library and vision demos will be skipped")
endif()

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
    message(WARNING "FFmpeg development libraries were not found; RTSP detection targets will be built without FFmpeg support")
endif()

add_library(honta_config_model
    code/src/config/runtime_config.cpp
    code/src/config/camera_config.cpp
    code/src/config/config_loader.cpp
    code/src/model/top_cover_model.cpp
)

target_include_directories(honta_config_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/code/third_party
)

add_library(honta_detection_types
    code/src/vision/detection_types.cpp
)

target_include_directories(honta_detection_types
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
)

add_library(honta_calibration
    code/src/calibration/coordinate_solver.cpp
    code/src/calibration/calibration_session.cpp
)

target_include_directories(honta_calibration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
)

target_link_libraries(honta_calibration
    PUBLIC
        honta_config_model
        honta_detection_types
)

if(HONTA_HAS_OPENCV)
    add_library(honta_vision
        code/src/vision/circle_detector.cpp
        code/src/vision/calibration_bridge.cpp
        code/src/vision/frame_crop.cpp
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
            honta_detection_types
            honta_calibration
        PRIVATE
            honta_logging
            honta_config_model
    )

    if(HONTA_HAS_FFMPEG)
        target_link_libraries(honta_vision
            PRIVATE
                ${HONTA_FFMPEG_LIBS}
        )
    endif()

    add_executable(calibrate_camera_tool
        code/tools/calibrate_camera_tool.cpp
    )

    target_link_libraries(calibrate_camera_tool
        PRIVATE
            ${OpenCV_LIBS}
    )

    target_include_directories(calibrate_camera_tool
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/code/third_party
    )

    add_executable(capture_calibration_images_tool
        code/tools/capture_calibration_images_tool.cpp
    )

    target_link_libraries(capture_calibration_images_tool
        PRIVATE
            ${OpenCV_LIBS}
    )
endif()

add_library(honta_native SHARED
    code/src/api/honta_api.cpp
    code/src/api/honta_context.cpp
)

target_compile_definitions(honta_native
    PRIVATE
        HONTA_API_BUILD
)

if(HONTA_ENABLE_MOCK_DETECTION)
    target_compile_definitions(honta_native
        PRIVATE
            HONTA_ENABLE_MOCK_DETECTION
    )
endif()

if(HONTA_HAS_OPENCV AND HONTA_HAS_FFMPEG)
    target_compile_definitions(honta_native
        PRIVATE
            HONTA_HAS_REAL_DETECTION
    )
endif()

target_include_directories(honta_native
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/code/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/code/src/api
)

target_link_libraries(honta_native
    PRIVATE
        honta_logging
        honta_config_model
        honta_calibration
)

if(HONTA_HAS_OPENCV AND HONTA_HAS_FFMPEG)
    target_link_libraries(honta_native
        PRIVATE
            honta_vision
    )
endif()

if(BUILD_TESTING AND NOT ANDROID)
    add_executable(honta_native_log_test
        code/tests/native_log_test.cpp
    )

    target_include_directories(honta_native_log_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/code/include
    )

    target_link_libraries(honta_native_log_test
        PRIVATE
            honta_logging
    )

    add_test(NAME honta_native_log_test COMMAND honta_native_log_test)

    if(HONTA_HAS_OPENCV AND HONTA_HAS_FFMPEG)
        add_executable(honta_detection_service_error_test
            code/tests/detection_service_error_test.cpp
        )

        target_link_libraries(honta_detection_service_error_test
            PRIVATE
                honta_vision
        )

        add_test(
            NAME honta_detection_service_error_test
            COMMAND honta_detection_service_error_test
        )

        add_executable(honta_c_api_background_error_test
            code/tests/c_api_background_error_test.cpp
        )

        target_link_libraries(honta_c_api_background_error_test
            PRIVATE
                honta_native
        )

        add_test(
            NAME honta_c_api_background_error_test
            COMMAND honta_c_api_background_error_test
                ${CMAKE_CURRENT_SOURCE_DIR}/code/tests/data/native_error_runtime.json
        )
    endif()
endif()
