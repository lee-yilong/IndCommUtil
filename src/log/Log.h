#pragma once

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <wx/string.h>
#include <wx/textctrl.h>

#include <string>

// =========================================================================
// 为 wxString 定义 fmt formatter
//
// 目的：让 SPDLOG_INFO(_("...")) 或 SPDLOG_INFO("{}", wxStr) 能编译。
// 转换规则：wxString -> UTF-8 std::string
// =========================================================================
template <>
struct fmt::formatter<wxString> : fmt::formatter<std::string> {
    auto format(const wxString& s, format_context& ctx) const {
        return fmt::formatter<std::string>::format(std::string(s.utf8_str()), ctx);
    }
};

namespace icu::log {

void Init();
void Shutdown();

// 挂载/卸载 GUI sink（日志窗口打开时挂载，关闭时卸载）
void AttachGuiSink(wxTextCtrl* ctrl);
void DetachGuiSink();

}  // namespace icu::log

// =========================================================================
// 日志翻译宏
//
// 用法约定：
//   - 无参数 → SPDLOG_INFO(_("..."))            （走 formatter，wxString 自动转 UTF-8）
//   - 有参数 → ICU_SPDLOG_INFO(_("... %s ..."), ...)（走 wxString::Format）
// =========================================================================

#define ICU_SPDLOG_INFO(fmt, ...) \
    SPDLOG_INFO("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_WARN(fmt, ...) \
    SPDLOG_WARN("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_ERROR(fmt, ...) \
    SPDLOG_ERROR("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))

#define ICU_SPDLOG_DEBUG(fmt, ...) \
    SPDLOG_DEBUG("{}", std::string(wxString::Format(fmt, ##__VA_ARGS__).utf8_str()))
