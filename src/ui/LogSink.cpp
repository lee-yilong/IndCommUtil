#include "LogSink.h"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <deque>
#include <mutex>

namespace icu::log {

namespace {

std::mutex g_historyMutex;
std::deque<wxString> g_history;
const size_t MAX_HISTORY = 1000;

template <typename Mutex>
class WxTextCtrlSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit WxTextCtrlSink(wxTextCtrl* textCtrl) : m_textCtrl(textCtrl) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        wxString wxMsg = wxString::FromUTF8(formatted.data(), formatted.size());

        // 存入历史
        {
            std::lock_guard<std::mutex> lock(g_historyMutex);
            g_history.push_back(wxMsg);
            if (g_history.size() > MAX_HISTORY) g_history.pop_front();
        }

        wxTextCtrl* ctrl = m_textCtrl;
        ctrl->CallAfter([ctrl, wxMsg]() {
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
std::shared_ptr<WxTextCtrlSink_mt> g_guiSink;

}  // namespace

void AttachGuiSink(wxTextCtrl* ctrl) {
    DetachGuiSink();
    if (!ctrl) return;
    auto logger = spdlog::default_logger();
    if (!logger) return;

    g_guiSink = std::make_shared<WxTextCtrlSink_mt>(ctrl);
    logger->sinks().push_back(g_guiSink);
}

void DetachGuiSink() {
    auto logger = spdlog::default_logger();
    if (g_guiSink && logger) {
        auto& sinks = logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), g_guiSink), sinks.end());
    }
    g_guiSink.reset();
}

std::vector<wxString> GetLogHistory() {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    return std::vector<wxString>(g_history.begin(), g_history.end());
}

void ClearLogHistory() {
    std::lock_guard<std::mutex> lock(g_historyMutex);
    g_history.clear();
}

}  // namespace icu::log
