#pragma once

#ifdef _WIN32
#ifdef HONTA_API_BUILD
#define HONTA_API __declspec(dllexport)
#else
#define HONTA_API __declspec(dllimport)
#endif
#else
#define HONTA_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum HontaStatus {
    HONTA_OK = 0,
    HONTA_ERROR_INVALID_ARGUMENT = 1,
    HONTA_ERROR_NOT_INITIALIZED = 2,
    HONTA_ERROR_BAD_STATE = 3,
    HONTA_ERROR_CONFIG = 4,
    HONTA_ERROR_DETECTION = 5,
    HONTA_ERROR_CALIBRATION = 6,
    HONTA_ERROR_BUFFER_TOO_SMALL = 7,
    HONTA_ERROR_INTERNAL = 100
} HontaStatus;

HONTA_API int honta_init(const char* config_path);

HONTA_API int honta_set_top_cover_model(const char* model_id);

HONTA_API int honta_start_detect(const char* detect_type);

HONTA_API int honta_get_overlay_rtsp_url(
    char* output_text,
    int output_text_size,
    int* required_size
);

HONTA_API int honta_stop_detect();

HONTA_API int honta_confirm_entrance_candidate(
    int candidate_id,
    int entrance_hole_no,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw
);

HONTA_API int honta_confirm_center_candidate(
    int candidate_id,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw
);

HONTA_API int honta_calculate_all_horns();

HONTA_API int honta_get_horn_coordinate(
    int horn_no,
    double* x,
    double* y
);

HONTA_API int honta_get_last_error(
    char* output_text,
    int output_text_size,
    int* required_size
);

HONTA_API void honta_release();

#ifdef __cplusplus
}
#endif
