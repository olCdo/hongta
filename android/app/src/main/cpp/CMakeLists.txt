cmake_minimum_required(VERSION 3.22.1)

project(honta_android_jni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HONTA_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(HONTA_NATIVE_SO
    "${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}/libhonta_native.so")

if(NOT EXISTS "${HONTA_NATIVE_SO}")
    message(FATAL_ERROR
        "Missing ${HONTA_NATIVE_SO}. Build or copy libhonta_native.so and its "
        "dependencies into app/src/main/jniLibs/${ANDROID_ABI} before building "
        "the Android app.")
endif()

add_library(honta_native SHARED IMPORTED)
set_target_properties(honta_native PROPERTIES
    IMPORTED_LOCATION "${HONTA_NATIVE_SO}")

add_library(honta_jni SHARED
    HontaNativeBridge.cpp)

target_include_directories(honta_jni
    PRIVATE
        "${HONTA_REPO_ROOT}/code/include")

find_library(log-lib log)

target_link_libraries(honta_jni
    PRIVATE
        honta_native
        ${log-lib})
