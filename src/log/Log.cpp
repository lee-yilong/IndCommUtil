#include "Log.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace icu::log {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
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

    g_logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    spdlog::set_default_logger(g_logger);
    spdlog::set_level(spdlog::level::debug);
    g_logger->flush_on(spdlog::level::trace);
}

void Shutdown() {
    spdlog::shutdown();
}

}  // namespace icu::log
