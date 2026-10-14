#include "Log.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace icu::log {

namespace {

// GUI sink：把日志转发到 wxTextCtrl
template <typename Mutex>
class WxTextCtrlSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit WxTextCtrlSink(wxTextCtrl* textCtrl) : m_textCtrl(textCtrl) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        wxString wxMsg = wxString::FromUTF8(formatted.data(), formatted.size());

        // 捕获指针，CallAfter 时先判断窗口是否还在
        wxTextCtrl* ctrl = m_textCtrl;
        ctrl->CallAfter([ctrl, wxMsg]() {
            // wxWidgets 里窗口销毁后指针不可用；用 IsBeingDeleted 判断
            if (ctrl && !ctrl->IsBeingDeleted()) {
                ctrl->AppendText(wxMsg + "\n");
                ctrl->SetInsertionPointEnd();
            }
        });
    }

    void flush_() override {}

private:
    wxTextCtrl* m_textCtrl;
};

using WxTextCtrlSink_mt = WxTextCtrlSink<std::mutex>;

std::shared_ptr<spdlog::logger> g_logger;
std::shared_ptr<WxTextCtrlSink_mt> g_guiSink;

}  // namespace

void Init() {
    std::vector<spdlog::sink_ptr> sinks;

    // 控制台 sink
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    // 文件 sink
    std::filesystem::create_directories("logs");
    sinks.push_back(
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/app.log", 1024 * 1024 * 5, 3));

    g_logger = std::make_shared<spdlog::logger>("app", sinks.begin(), sinks.end());
    spdlog::set_default_logger(g_logger);
    spdlog::set_level(spdlog::level::debug);
    g_logger->flush_on(spdlog::level::err);
}

void Shutdown() {
    DetachGuiSink();
    spdlog::shutdown();
}

void AttachGuiSink(wxTextCtrl* ctrl) {
    DetachGuiSink();
    if (!ctrl || !g_logger) return;

    g_guiSink = std::make_shared<WxTextCtrlSink_mt>(ctrl);
    g_logger->sinks().push_back(g_guiSink);
}

void DetachGuiSink() {
    if (g_guiSink && g_logger) {
        auto& sinks = g_logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), g_guiSink), sinks.end());
    }
    g_guiSink.reset();
}

}  // namespace icu::log
