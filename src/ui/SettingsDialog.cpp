#include "SettingsDialog.h"

#include <wx/statbox.h>
#include <wx/stattext.h>

#include "settings/UISettings.h"

namespace icu {

SettingsDialog::SettingsDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Application Settings"), wxDefaultPosition, wxSize(450, 350)) {
    BuildUi();
    LoadSettings();
    CentreOnParent();
}

void SettingsDialog::BuildUi() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_notebook = new wxNotebook(this, wxID_ANY);

    // --- 通用设置页 ---
    auto* generalPage = new wxPanel(m_notebook);
    auto* generalSizer = new wxBoxSizer(wxVERTICAL);
    auto* generalBox = new wxStaticBoxSizer(wxVERTICAL, generalPage, _("Window Behavior"));

    m_minimizeToTray = new wxCheckBox(generalBox->GetStaticBox(), wxID_ANY,
                                      _("Minimize to system tray when closing"));
    generalBox->Add(m_minimizeToTray, 0, wxALL, 8);

    generalSizer->Add(generalBox, 0, wxEXPAND | wxALL, 10);
    generalPage->SetSizer(generalSizer);
    m_notebook->AddPage(generalPage, _("General"));

    // --- 界面与日志设置页 ---
    auto* uiPage = new wxPanel(m_notebook);
    auto* uiSizer = new wxBoxSizer(wxVERTICAL);

    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, uiPage, _("Log Display"));
    m_hexDisplay =
        new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Show data in HEX format by default"));
    m_autoScroll = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Auto scroll to bottom"));
    m_showTimestamp = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Show timestamp prefix"));

    logBox->Add(m_hexDisplay, 0, wxALL, 8);
    logBox->Add(m_autoScroll, 0, wxALL, 8);
    logBox->Add(m_showTimestamp, 0, wxALL, 8);
    uiSizer->Add(logBox, 0, wxEXPAND | wxALL, 10);

    auto* fontBox = new wxStaticBoxSizer(wxVERTICAL, uiPage, _("Fonts"));
    auto* fontGrid = new wxFlexGridSizer(2, 8, 8);
    fontGrid->Add(new wxStaticText(fontBox->GetStaticBox(), wxID_ANY, _("Font Size (pt):")), 0,
                  wxALIGN_CENTER_VERTICAL);
    m_fontSizeCtrl = new wxSpinCtrl(fontBox->GetStaticBox(), wxID_ANY, "", wxDefaultPosition,
                                    wxDefaultSize, wxSP_ARROW_KEYS, 8, 24, 10);
    fontGrid->Add(m_fontSizeCtrl, 0);
    fontBox->Add(fontGrid, 1, wxEXPAND | wxALL, 8);
    uiSizer->Add(fontBox, 0, wxEXPAND | wxALL, 10);

    uiPage->SetSizer(uiSizer);
    m_notebook->AddPage(uiPage, _("Interface & Log"));

    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);

    // --- 底部按钮 ---
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnApply = new wxButton(this, wxID_APPLY, _("Apply"));
    auto* btnOk = new wxButton(this, wxID_OK, _("OK"));
    auto* btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));

    btnApply->Bind(wxEVT_BUTTON, &SettingsDialog::OnApply, this);
    btnOk->Bind(wxEVT_BUTTON, &SettingsDialog::OnOk, this);
    btnCancel->Bind(wxEVT_BUTTON, &SettingsDialog::OnCancel, this);

    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnApply, 0, wxALL, 5);
    btnSizer->Add(btnOk, 0, wxALL, 5);
    btnSizer->Add(btnCancel, 0, wxALL, 5);
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

void SettingsDialog::OnApply(wxCommandEvent&) {
    SaveSettings();
}

void SettingsDialog::OnOk(wxCommandEvent&) {
    SaveSettings();
    EndModal(wxID_OK);
}

void SettingsDialog::OnCancel(wxCommandEvent&) {
    EndModal(wxID_CANCEL);
}

}  // namespace icu
