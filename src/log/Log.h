#pragma once

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <wx/string.h>
#include <wx/translation.h>

#include <string>

// 为 wxString 定义 fmt formatter
template <>
struct fmt::formatter<wxString> : fmt::formatter<std::string> {
    auto format(const wxString& s, format_context& ctx) const {
        return fmt::formatter<std::string>::format(std::string(s.utf8_str()), ctx);
    }
};

namespace icu::log {
void Init();
void Shutdown();
}  // namespace icu::log

// 日志翻译宏
#define ICU_SPDLOG_INFO(fmt, ...) \
    SPDLOG_INFO("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_WARN(fmt, ...) \
    SPDLOG_WARN("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_ERROR(fmt, ...) \
    SPDLOG_ERROR("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_DEBUG(fmt, ...) \
    SPDLOG_DEBUG("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))
