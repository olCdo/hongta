#include "honta/api/honta_api.h"

#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>

#include "honta_context.h"

namespace {

std::mutex g_mutex;
std::unique_ptr<honta::api::HontaContext> g_context;
std::string g_last_error;

void setGlobalError(const std::string& error) {
    g_last_error = error;
    if (g_context) {
        g_context->setLastError(error);
    }
}

int writeText(const std::string& text,
              char* output_text,
              int output_text_size,
              int* required_size) {
    if (required_size == nullptr || output_text_size < 0) {
        return HONTA_ERROR_INVALID_ARGUMENT;
    }
    const int required = static_cast<int>(text.size()) + 1;
    *required_size = required;
    if (output_text == nullptr || output_text_size < required) {
        return HONTA_ERROR_BUFFER_TOO_SMALL;
    }
    std::memcpy(output_text, text.c_str(), static_cast<std::size_t>(required));
    return HONTA_OK;
}

int requireContext() {
    if (!g_context) {
        setGlobalError("honta context is not initialized");
        return HONTA_ERROR_NOT_INITIALIZED;
    }
    return HONTA_OK;
}

int catchInternal(const std::exception& error) {
    setGlobalError(std::string("internal error: ") + error.what());
    return HONTA_ERROR_INTERNAL;
}

int syncResultError(int status) {
    if (status != HONTA_OK && g_context) {
        g_last_error = g_context->lastError();
    }
    return status;
}

}  // namespace

extern "C" {

HONTA_API int honta_init(const char* config_path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (config_path == nullptr || config_path[0] == '\0') {
        setGlobalError("config_path is required");
        return HONTA_ERROR_INVALID_ARGUMENT;
    }
    try {
        g_context.reset();
        g_context = std::make_unique<honta::api::HontaContext>(config_path);
        g_last_error.clear();
        return HONTA_OK;
    } catch (const std::exception& error) {
        setGlobalError(std::string("failed to initialize honta context: ") + error.what());
        return HONTA_ERROR_CONFIG;
    }
}

HONTA_API int honta_set_top_cover_model(const char* model_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->setTopCoverModel(model_id));
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_start_detect(const char* detect_type) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->startDetect(detect_type));
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_get_overlay_rtsp_url(
    char* output_text,
    int output_text_size,
    int* required_size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        const int status = writeText(g_context->overlayRtspUrl(),
                                     output_text,
                                     output_text_size,
                                     required_size);
        if (status == HONTA_ERROR_INVALID_ARGUMENT) {
            setGlobalError("invalid output buffer arguments");
        } else if (status == HONTA_ERROR_BUFFER_TOO_SMALL) {
            setGlobalError("output buffer is too small");
        }
        return status;
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_stop_detect() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->stopDetect());
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_confirm_entrance_candidate(
    int candidate_id,
    int entrance_hole_no,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->confirmEntranceCandidate(
            candidate_id, entrance_hole_no, height_mm, amr_x, amr_y, amr_yaw));
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_confirm_center_candidate(
    int candidate_id,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->confirmCenterCandidate(
            candidate_id, height_mm, amr_x, amr_y, amr_yaw));
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_calculate_all_horns() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->calculateAllHorns());
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_get_horn_coordinate(
    int horn_no,
    double* x,
    double* y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (const int status = requireContext(); status != HONTA_OK) {
        return status;
    }
    try {
        return syncResultError(g_context->getHornCoordinate(horn_no, x, y));
    } catch (const std::exception& error) {
        return catchInternal(error);
    }
}

HONTA_API int honta_get_last_error(
    char* output_text,
    int output_text_size,
    int* required_size) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_context) {
        g_last_error = g_context->lastError();
    }
    return writeText(g_last_error, output_text, output_text_size, required_size);
}

HONTA_API void honta_release() {
    std::lock_guard<std::mutex> lock(g_mutex);
    try {
        g_context.reset();
    } catch (const std::exception& error) {
        g_context.reset();
        g_last_error = std::string("failed to release honta context: ") + error.what();
    } catch (...) {
        g_context.reset();
        g_last_error = "failed to release honta context: unknown error";
    }
}

}  // extern "C"
