#include "LogFrame.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "LogSink.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"
#include "ui/UIUtils.h"

namespace icu {

LogFrame::LogFrame(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Log"), wxDefaultPosition, wxSize(800, 550),
               wxBORDER_NONE | wxRESIZE_BORDER) {  // 允许调整大小
    SetBackgroundColour(ui::kBgColor);
    BuildUi();

    auto history = log::GetHistory();
    for (const auto& msg : history) {
        m_textCtrl->AppendText(msg + "\n");
    }
    m_textCtrl->SetInsertionPointEnd();

    log::AttachGuiSink(m_textCtrl);
    CentreOnParent();

    SPDLOG_INFO(_("Log window opened"));
}

LogFrame::~LogFrame() {
    log::DetachGuiSink();
}

void LogFrame::BuildUi() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. 自定义标题栏
    auto* header = new wxPanel(this);
    header->SetBackgroundColour(ui::kBgColor);
    auto* titleSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* titleText = new wxStaticText(header, wxID_ANY, _("Log"));
    wxFont titleFont = titleText->GetFont();
    titleFont.SetPointSize(12);
    titleFont.MakeBold();
    titleText->SetFont(titleFont);
    titleText->SetForegroundColour(ui::kThemeDark);

    auto* closeBtn =
        new wxStaticBitmap(header, wxID_ANY, ui::LoadSvgIcon("close", 16, ui::kSubTextColor));
    closeBtn->SetCursor(wxCursor(wxCURSOR_HAND));
    closeBtn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { Hide(); });

    closeBtn->Bind(wxEVT_ENTER_WINDOW, [closeBtn](wxMouseEvent& evt) {
        closeBtn->SetBitmap(ui::LoadSvgIcon("close", 16, ui::kCloseHoverColor));
        evt.Skip();
    });
    closeBtn->Bind(wxEVT_LEAVE_WINDOW, [closeBtn](wxMouseEvent& evt) {
        closeBtn->SetBitmap(ui::LoadSvgIcon("close", 16, ui::kSubTextColor));
        evt.Skip();
    });

    titleSizer->AddStretchSpacer(1);
    titleSizer->Add(titleText, 0, wxALIGN_CENTER_VERTICAL);
    titleSizer->AddStretchSpacer(1);
    titleSizer->Add(closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    header->SetSizer(titleSizer);
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    // 修复拖拽：将拖拽事件同时绑定到标题栏和文字上，防止文字吞噬点击
    auto bindDrag = [this](wxWindow* w, wxWindow* target) {
        w->Bind(wxEVT_LEFT_DOWN, [this, target](wxMouseEvent& evt) {
            if (evt.LeftIsDown()) {
                m_dragStart = wxGetMousePosition();
                m_winStart = GetPosition();
                if (!target->HasCapture()) target->CaptureMouse();
            }
        });
        w->Bind(wxEVT_MOTION, [this, target](wxMouseEvent& evt) {
            if (evt.LeftIsDown() && target->HasCapture()) {
                Move(m_winStart + wxGetMousePosition() - m_dragStart);
            }
            evt.Skip();
        });
        w->Bind(wxEVT_LEFT_UP, [target](wxMouseEvent&) {
            if (target->HasCapture()) target->ReleaseMouse();
        });
        w->Bind(wxEVT_MOUSE_CAPTURE_LOST, [target](wxMouseCaptureLostEvent&) {
            if (target->HasCapture()) target->ReleaseMouse();
        });
    };
    bindDrag(header, header);
    bindDrag(titleText, header);

    // 2. 主体卡片
    auto* card = new ui::CardPanel(this);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto* btnLoad =
        new ui::FlatButton(card, _("Load File..."), ui::kBtnGrayBg, ui::kTextColor, [this]() {
            wxCommandEvent e;
            OnLoad(e);
        });
    auto* btnClear = new ui::FlatButton(card, _("Clear"), ui::kBtnGrayBg, ui::kTextColor, [this]() {
        wxCommandEvent e;
        OnClear(e);
    });

    m_autoScroll = new wxCheckBox(card, wxID_ANY, _("Auto Scroll"));
    m_autoScroll->SetValue(true);
    m_autoScroll->SetForegroundColour(ui::kTextColor);

    toolbar->Add(btnLoad, 0, wxALL, 8);
    toolbar->Add(btnClear, 0, wxALL, 8);
    toolbar->Add(m_autoScroll, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

    cardSizer->Add(toolbar, 0, wxEXPAND);

    m_textCtrl = new wxTextCtrl(card, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    wxFont mono(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_textCtrl->SetFont(mono);
    cardSizer->Add(m_textCtrl, 1, wxEXPAND | wxALL, 8);

    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // 3. 底部留白
    mainSizer->AddSpacer(16);

    SetSizer(mainSizer);
    Layout();
}

void LogFrame::OnClose(wxCloseEvent& evt) {
    evt.Veto();
    Hide();
}

void LogFrame::OnClear(wxCommandEvent&) {
    m_textCtrl->Clear();
    log::ClearHistory();
}

void LogFrame::OnLoad(wxCommandEvent&) {
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    exePath.SetFullName("");
    wxString logDir = exePath.GetPath() + "/logs";

    if (!wxFileName::DirExists(logDir)) {
        logDir = exePath.GetPath();
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
                m_textCtrl->SetInsertionPoint(0);
                if (m_autoScroll) m_autoScroll->SetValue(false);
            }
            ICU_SPDLOG_INFO(_("Loaded log file: %s"), path);
        } else {
            ui::MessageDialog::Show(this, _("Error"), _("Failed to open log file."),
                                    wxOK | wxICON_ERROR);
        }
    }
}

}  // namespace icu
