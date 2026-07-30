#pragma once

#include <string>
#include <string_view>

namespace honta::logging {

std::string redactRtspCredentials(std::string_view value);

void info(std::string_view component, std::string_view message);
void warning(std::string_view component, std::string_view message);
void error(std::string_view component, std::string_view message);

}  // namespace honta::logging
