#include "LogFrame.h"

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

// clang-format off
// wxWidgets 事件表宏展开后是一条语句，clang-format 会把后续行的缩进逐级累积
wxBEGIN_EVENT_TABLE(LogFrame, wxFrame) EVT_CLOSE(LogFrame::OnClose)
    EVT_BUTTON(wxID_ANY, LogFrame::OnClear) wxEND_EVENT_TABLE();  // clang-format on

LogFrame::LogFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, _("Log"), wxDefaultPosition, wxSize(900, 500),
              wxDEFAULT_FRAME_STYLE) {
    BuildUi();

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

    m_clearBtn = new wxButton(panel, wxID_ANY, _("Clear"));
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
}

void LogFrame::ForceDestroy() {
    m_forceDestroy = true;
    Destroy();
}

}  // namespace icu
