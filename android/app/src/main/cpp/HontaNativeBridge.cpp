#include <jni.h>

#include <string>
#include <vector>

#include "honta/api/honta_api.h"

namespace {

std::string toString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jstring toJString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

std::string readTextWithBufferProtocol(
        int (*function)(char*, int, int*)) {
    int required = 0;
    const int probe = function(nullptr, 0, &required);
    if (probe != HONTA_ERROR_BUFFER_TOO_SMALL && probe != HONTA_OK) {
        return {};
    }
    if (required <= 0) {
        return {};
    }
    std::vector<char> buffer(static_cast<std::size_t>(required));
    const int status = function(buffer.data(), required, &required);
    if (status != HONTA_OK) {
        return {};
    }
    return buffer.data();
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_honta_vision_HontaNative_bridgeVersion(JNIEnv* env, jclass) {
    return toJString(env, "Honta JNI bridge 0.1.0-phase7");
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeInit(
        JNIEnv* env, jclass, jstring config_path) {
    const std::string value = toString(env, config_path);
    return honta_init(value.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeSetTopCoverModel(
        JNIEnv* env, jclass, jstring model_id) {
    const std::string value = toString(env, model_id);
    return honta_set_top_cover_model(value.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeStartDetect(
        JNIEnv* env, jclass, jstring detect_type) {
    const std::string value = toString(env, detect_type);
    return honta_start_detect(value.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_honta_vision_HontaNative_nativeGetOverlayRtspUrl(JNIEnv* env, jclass) {
    return toJString(env, readTextWithBufferProtocol(honta_get_overlay_rtsp_url));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeStopDetect(JNIEnv*, jclass) {
    return honta_stop_detect();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeConfirmEntranceCandidate(
        JNIEnv*, jclass,
        jint candidate_id,
        jint entrance_hole_no,
        jdouble height_mm,
        jdouble amr_x,
        jdouble amr_y,
        jdouble amr_yaw) {
    return honta_confirm_entrance_candidate(
            candidate_id,
            entrance_hole_no,
            height_mm,
            amr_x,
            amr_y,
            amr_yaw);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeConfirmCenterCandidate(
        JNIEnv*, jclass,
        jint candidate_id,
        jdouble height_mm,
        jdouble amr_x,
        jdouble amr_y,
        jdouble amr_yaw) {
    return honta_confirm_center_candidate(
            candidate_id,
            height_mm,
            amr_x,
            amr_y,
            amr_yaw);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_honta_vision_HontaNative_nativeCalculateAllHorns(JNIEnv*, jclass) {
    return honta_calculate_all_horns();
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_honta_vision_HontaNative_nativeGetHornCoordinate(
        JNIEnv* env, jclass, jint horn_no) {
    double x = 0.0;
    double y = 0.0;
    const int status = honta_get_horn_coordinate(horn_no, &x, &y);
    if (status != HONTA_OK) {
        jclass exception_class = env->FindClass(
                "com/honta/vision/HontaNative$HontaNativeException");
        if (exception_class != nullptr) {
            std::string error = readTextWithBufferProtocol(honta_get_last_error);
            env->ThrowNew(exception_class, error.c_str());
        }
        return nullptr;
    }
    jdouble values[2] = {x, y};
    jdoubleArray result = env->NewDoubleArray(2);
    if (result != nullptr) {
        env->SetDoubleArrayRegion(result, 0, 2, values);
    }
    return result;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_honta_vision_HontaNative_nativeGetLastError(JNIEnv* env, jclass) {
    return toJString(env, readTextWithBufferProtocol(honta_get_last_error));
}

extern "C" JNIEXPORT void JNICALL
Java_com_honta_vision_HontaNative_nativeRelease(JNIEnv*, jclass) {
    honta_release();
}
