#include "SettingsDialog.h"

#include <wx/statbox.h>
#include <wx/stattext.h>

#include "settings/UISettings.h"
#include "ui/UIUtils.h"

namespace icu {

SettingsDialog::SettingsDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Application Settings"), wxDefaultPosition, wxSize(450, 380),
               wxBORDER_NONE | wxRESIZE_BORDER) {
    SetBackgroundColour(ui::kBgColor);
    BuildUi();
    LoadSettings();
    CentreOnParent();
}

void SettingsDialog::BuildUi() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. 顶部标题栏 (拖拽 + 关闭)
    auto* header = ui::CreateDialogHeader(this, _("Application Settings"), m_dragStart, m_winStart,
                                          [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    // 2. 内容区（使用 wxNotebook 保持切换）
    m_notebook = new wxNotebook(this, wxID_ANY);
    m_notebook->SetBackgroundColour(ui::kBgColor);

    // --- 通用设置页 ---
    auto* generalPage = new wxPanel(m_notebook);
    generalPage->SetBackgroundColour(ui::kBgColor);
    auto* generalSizer = new wxBoxSizer(wxVERTICAL);

    auto* genCard = new ui::CardPanel(generalPage);
    auto* genCardSizer = new wxBoxSizer(wxVERTICAL);

    auto* titleGen = new wxStaticText(genCard, wxID_ANY, _("Window Behavior"));
    titleGen->SetFont(titleGen->GetFont().Bold());
    titleGen->SetForegroundColour(ui::kThemeDark);
    genCardSizer->Add(titleGen, 0, wxALL, 12);

    m_minimizeToTray = new wxCheckBox(genCard, wxID_ANY, _("Minimize to system tray when closing"));
    m_minimizeToTray->SetForegroundColour(ui::kTextColor);
    genCardSizer->Add(m_minimizeToTray, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    genCard->SetSizer(genCardSizer);
    generalSizer->Add(genCard, 0, wxEXPAND | wxALL, 10);
    generalPage->SetSizer(generalSizer);
    m_notebook->AddPage(generalPage, _("General"));

    // --- 界面与日志设置页 ---
    auto* uiPage = new wxPanel(m_notebook);
    uiPage->SetBackgroundColour(ui::kBgColor);
    auto* uiSizer = new wxBoxSizer(wxVERTICAL);

    auto* logCard = new ui::CardPanel(uiPage);
    auto* logCardSizer = new wxBoxSizer(wxVERTICAL);

    auto* titleLog = new wxStaticText(logCard, wxID_ANY, _("Log Display"));
    titleLog->SetFont(titleLog->GetFont().Bold());
    titleLog->SetForegroundColour(ui::kThemeDark);
    logCardSizer->Add(titleLog, 0, wxALL, 12);

    m_hexDisplay = new wxCheckBox(logCard, wxID_ANY, _("Show data in HEX format by default"));
    m_hexDisplay->SetForegroundColour(ui::kTextColor);
    logCardSizer->Add(m_hexDisplay, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_autoScroll = new wxCheckBox(logCard, wxID_ANY, _("Auto scroll to bottom"));
    m_autoScroll->SetForegroundColour(ui::kTextColor);
    logCardSizer->Add(m_autoScroll, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_showTimestamp = new wxCheckBox(logCard, wxID_ANY, _("Show timestamp prefix"));
    m_showTimestamp->SetForegroundColour(ui::kTextColor);
    logCardSizer->Add(m_showTimestamp, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    logCard->SetSizer(logCardSizer);
    uiSizer->Add(logCard, 0, wxEXPAND | wxALL, 10);

    auto* fontCard = new ui::CardPanel(uiPage);
    auto* fontCardSizer = new wxBoxSizer(wxVERTICAL);

    auto* titleFont = new wxStaticText(fontCard, wxID_ANY, _("Fonts"));
    titleFont->SetFont(titleFont->GetFont().Bold());
    titleFont->SetForegroundColour(ui::kThemeDark);
    fontCardSizer->Add(titleFont, 0, wxALL, 12);

    auto* fontGrid = new wxFlexGridSizer(2, 8, 8);
    auto* lblSize = new wxStaticText(fontCard, wxID_ANY, _("Font Size (pt):"));
    lblSize->SetForegroundColour(ui::kTextColor);
    fontGrid->Add(lblSize, 0, wxALIGN_CENTER_VERTICAL);

    m_fontSizeCtrl = new wxSpinCtrl(fontCard, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 8, 24, 10);
    fontGrid->Add(m_fontSizeCtrl, 0);

    fontCardSizer->Add(fontGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    fontCard->SetSizer(fontCardSizer);
    uiSizer->Add(fontCard, 0, wxEXPAND | wxALL, 10);

    uiPage->SetSizer(uiSizer);
    m_notebook->AddPage(uiPage, _("Interface & Log"));

    mainSizer->Add(m_notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // 3. 底部按钮区 (全用 FlatButton)
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* btnApply = new ui::FlatButton(this, _("Apply"), ui::kBtnGrayBg, ui::kTextColor,
                                        [this]() { SaveSettings(); });
    auto* btnOk = new ui::FlatButton(this, _("OK"), ui::kThemeBlue, *wxWHITE, [this]() {
        SaveSettings();
        EndModal(wxID_OK);
    });
    auto* btnCancel = new ui::FlatButton(this, _("Cancel"), ui::kBtnGrayBg, ui::kTextColor,
                                         [this]() { EndModal(wxID_CANCEL); });

    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnApply, 0, wxALL, 6);
    btnSizer->Add(btnOk, 0, wxALL, 6);
    btnSizer->Add(btnCancel, 0, wxALL, 6);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);

    SetSizer(mainSizer);
}

void SettingsDialog::LoadSettings() {
    auto& ui = settings::UISettings::Instance();
    m_minimizeToTray->SetValue(ui.GetMinimizeToTrayOnClose());
    m_fontSizeCtrl->SetValue(ui.GetFontSize());
    m_hexDisplay->SetValue(ui.GetHexDisplay());
    m_autoScroll->SetValue(ui.GetAutoScroll());
    m_showTimestamp->SetValue(ui.GetShowTimestamp());
}

void SettingsDialog::SaveSettings() {
    auto& ui = settings::UISettings::Instance();
    ui.SetMinimizeToTrayOnClose(m_minimizeToTray->GetValue());
    ui.SetFontSize(m_fontSizeCtrl->GetValue());
    ui.SetHexDisplay(m_hexDisplay->GetValue());
    ui.SetAutoScroll(m_autoScroll->GetValue());
    ui.SetShowTimestamp(m_showTimestamp->GetValue());

    if (m_onApply) m_onApply();
}

}  // namespace icu
