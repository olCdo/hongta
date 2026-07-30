#include "honta/logging/native_log.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace honta::logging {
namespace {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

bool hasCaseInsensitivePrefix(std::string_view value,
                              std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    return std::equal(
        prefix.begin(),
        prefix.end(),
        value.begin(),
        [](char expected, char actual) {
            return expected == static_cast<char>(
                                   std::tolower(static_cast<unsigned char>(actual)));
        });
}

std::size_t rtspAuthorityBegin(std::string_view value) {
    constexpr std::string_view rtsp_prefix = "rtsp://";
    constexpr std::string_view rtsps_prefix = "rtsps://";
    if (hasCaseInsensitivePrefix(value, rtsp_prefix)) {
        return rtsp_prefix.size();
    }
    if (hasCaseInsensitivePrefix(value, rtsps_prefix)) {
        return rtsps_prefix.size();
    }
    return std::string::npos;
}

void write(LogLevel level,
           std::string_view component,
           std::string_view message) {
#ifdef __ANDROID__
    int priority = ANDROID_LOG_INFO;
    if (level == LogLevel::Warning) {
        priority = ANDROID_LOG_WARN;
    } else if (level == LogLevel::Error) {
        priority = ANDROID_LOG_ERROR;
    }
    __android_log_print(
        priority,
        "HontaNative",
        "%.*s: %.*s",
        static_cast<int>(component.size()),
        component.data(),
        static_cast<int>(message.size()),
        message.data());
#else
    const char* level_name = "INFO";
    if (level == LogLevel::Warning) {
        level_name = "WARN";
    } else if (level == LogLevel::Error) {
        level_name = "ERROR";
    }
    std::cerr << "[HontaNative][" << level_name << "] "
              << component << ": " << message << '\n';
#endif
}

}  // namespace

std::string redactRtspCredentials(std::string_view value) {
    const std::size_t authority_begin = rtspAuthorityBegin(value);
    if (authority_begin == std::string::npos) {
        return std::string(value);
    }

    std::string redacted(value);
    const std::size_t authority_end =
        redacted.find_first_of("/?#", authority_begin);
    const std::size_t search_end =
        authority_end == std::string::npos ? redacted.size() : authority_end;
    const std::size_t at = redacted.rfind('@', search_end);
    if (at == std::string::npos || at < authority_begin || at >= search_end) {
        return redacted;
    }

    redacted.replace(authority_begin, at - authority_begin, "***");
    return redacted;
}

void info(std::string_view component, std::string_view message) {
    write(LogLevel::Info, component, message);
}

void warning(std::string_view component, std::string_view message) {
    write(LogLevel::Warning, component, message);
}

void error(std::string_view component, std::string_view message) {
    write(LogLevel::Error, component, message);
}

}  // namespace honta::logging
