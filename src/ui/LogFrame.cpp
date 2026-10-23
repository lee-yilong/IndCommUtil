#include "LogFrame.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "LogSink.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

wxBEGIN_EVENT_TABLE(LogFrame, wxFrame) EVT_CLOSE(LogFrame::OnClose)
    EVT_BUTTON(wxID_ANY, LogFrame::OnClear)
        EVT_BUTTON(wxID_ANY, LogFrame::OnLoad)  // 注意：这里的事件绑定需通过Bind区分
    wxEND_EVENT_TABLE()

        LogFrame::LogFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, _("Log"), wxDefaultPosition, wxSize(900, 500),
              wxDEFAULT_FRAME_STYLE) {
    BuildUi();

    // 加载内存中的历史日志
    auto history = log::GetHistory();
    for (const auto& msg : history) {
        m_textCtrl->AppendText(msg + "\n");
    }
    m_textCtrl->SetInsertionPointEnd();

    // 实时日志追加
    log::AttachGuiSink(m_textCtrl);
    CentreOnParent();

    SPDLOG_INFO(_("Log window opened"));
}

LogFrame::~LogFrame() {
    log::DetachGuiSink();
}

void LogFrame::BuildUi() {
    auto* panel = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);

    m_loadBtn = new wxButton(panel, wxID_ANY, _("Load File..."));
    m_loadBtn->Bind(wxEVT_BUTTON, &LogFrame::OnLoad, this);
    toolbar->Add(m_loadBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);

    m_clearBtn = new wxButton(panel, wxID_ANY, _("Clear"));
    m_clearBtn->Bind(wxEVT_BUTTON, &LogFrame::OnClear, this);
    toolbar->Add(m_clearBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);

    m_autoScroll = new wxCheckBox(panel, wxID_ANY, _("Auto Scroll"));
    m_autoScroll->SetValue(true);
    toolbar->Add(m_autoScroll, 0, wxALL | wxALIGN_CENTER_VERTICAL, 4);

    root->Add(toolbar, 0, wxEXPAND);

    m_textCtrl = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    wxFont mono(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_textCtrl->SetFont(mono);

    root->Add(m_textCtrl, 1, wxEXPAND | wxALL, 4);

    panel->SetSizer(root);
}

void LogFrame::OnClose(wxCloseEvent& evt) {
    if (m_forceDestroy) {
        evt.Skip();
        return;
    }

    evt.Veto();
    Hide();
    SPDLOG_INFO(_("Log window hidden"));
}

void LogFrame::OnClear(wxCommandEvent&) {
    m_textCtrl->Clear();
    log::ClearHistory();  // 清空内存缓存
}

void LogFrame::OnLoad(wxCommandEvent&) {
    // 默认定位到 exe 目录下的 logs 文件夹
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    exePath.SetFullName("");  // 去掉文件名，只保留目录
    wxString logDir = exePath.GetPath() + "/logs";

    if (!wxFileName::DirExists(logDir)) {
        logDir = exePath.GetPath();  // 如果 logs 文件夹不存在，退回到 exe 目录
    }

    wxFileDialog dlg(this, _("Open Log File"), logDir, "",
                     "Log files (*.log)|*.log|Text files (*.txt)|*.txt|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        wxFile file(path);
        if (file.IsOpened()) {
            wxString content;
            file.ReadAll(&content, wxConvUTF8);
            if (m_textCtrl) {
                m_textCtrl->SetValue(content);
                m_textCtrl->SetInsertionPoint(0);  // 滚动到顶部开始阅读
                if (m_autoScroll)
                    m_autoScroll->SetValue(false);  // 关闭自动滚动，防止新日志覆盖阅读
            }
            ICU_SPDLOG_INFO(_("Loaded log file: %s"), path);
        } else {
            wxMessageBox(_("Failed to open log file."), _("Error"), wxOK | wxICON_ERROR, this);
        }
    }
}

void LogFrame::ForceDestroy() {
    m_forceDestroy = true;
    Destroy();
}

}  // namespace icu
