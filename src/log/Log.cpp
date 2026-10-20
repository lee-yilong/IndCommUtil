#include "Log.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace icu::log {

namespace {
std::shared_ptr<spdlog::logger> g_logger;

// ★ 内存中的历史日志
std::mutex g_histMutex;
std::deque<wxString> g_history;
const size_t MAX_HISTORY = 2000;

class MemorySink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        wxString wxMsg = wxString::FromUTF8(formatted.data(), formatted.size());

        std::lock_guard<std::mutex> lock(g_histMutex);
        g_history.push_back(wxMsg);
        if (g_history.size() > MAX_HISTORY) g_history.pop_front();
    }
    void flush_() override {}
};
}  // namespace

void Init() {
    std::vector<spdlog::sink_ptr> sinks;

    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    const wxString logDirWx = exePath.GetPath() + wxFileName::GetPathSeparator() + "logs";
    const wxString logFileWx = logDirWx + wxFileName::GetPathSeparator() + "app.log";

    const std::string logDir = std::string(logDirWx.utf8_str());
    const std::string logFile = std::string(logFileWx.utf8_str());

    std::filesystem::create_directories(logDir);
    sinks.push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, 1024 * 1024 * 5, 3));

    // ★ 挂载内存记录器
    sinks.push_back(std::make_shared<MemorySink>());

    g_logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    spdlog::set_default_logger(g_logger);
    spdlog::set_level(spdlog::level::debug);
    g_logger->flush_on(spdlog::level::trace);
}

void Shutdown() {
    spdlog::shutdown();
}

std::vector<wxString> GetHistory() {
    std::lock_guard<std::mutex> lock(g_histMutex);
    return std::vector<wxString>(g_history.begin(), g_history.end());
}

void ClearHistory() {
    std::lock_guard<std::mutex> lock(g_histMutex);
    g_history.clear();
}

}  // namespace icu::log
