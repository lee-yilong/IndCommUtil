#include "TestStepEditDialog.h"

#include <wx/stattext.h>

#include "core/ValueValidator.h"
#include "ui/UIUtils.h"

namespace icu {

TestStepEditDialog::TestStepEditDialog(wxWindow* parent, const nlohmann::json& initialData,
                                       const std::vector<std::string>& availableVars,
                                       const std::map<std::string, std::string>& varTypes)
    : wxDialog(parent, wxID_ANY, _("Edit Test Step"), wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE | wxRESIZE_BORDER) {
    m_varNames = availableVars;
    m_varTypes = varTypes;
    SetBackgroundColour(ui::kBgColor);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. 标题栏
    auto* header = ui::CreateDialogHeader(this, _("Edit Test Step"), m_dragStart, m_winStart,
                                          [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    // 2. 主体卡片
    auto* card = new ui::CardPanel(this);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* grid = new wxFlexGridSizer(2, 10, 12);
    grid->AddGrowableCol(1, 1);

    // Step Name
    auto* lblName = new wxStaticText(card, wxID_ANY, _("Step Name:"));
    lblName->SetForegroundColour(ui::kTextColor);
    grid->Add(lblName, 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl =
        new wxTextCtrl(card, wxID_ANY, wxString::FromUTF8(initialData.value("name", "New Step")));
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    // Action
    auto* lblAction = new wxStaticText(card, wxID_ANY, _("Action:"));
    lblAction->SetForegroundColour(ui::kTextColor);
    grid->Add(lblAction, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString actions;
    actions.Add("read");
    actions.Add("write");
    actions.Add("delay");
    m_actionCtrl = new wxChoice(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, actions);

    std::string initAction = initialData.value("action", "read");
    if (initAction == "send") initAction = "write";  // 兼容旧数据
    m_actionCtrl->SetStringSelection(wxString::FromUTF8(initAction));
    if (m_actionCtrl->GetSelection() == wxNOT_FOUND) m_actionCtrl->SetSelection(0);
    m_actionCtrl->Bind(wxEVT_CHOICE, &TestStepEditDialog::OnActionChanged, this);
    grid->Add(m_actionCtrl, 1, wxEXPAND);

    // Variable
    m_varLabel = new wxStaticText(card, wxID_ANY, _("Variable:"));
    m_varLabel->SetForegroundColour(ui::kTextColor);
    grid->Add(m_varLabel, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString varChoices;
    for (const auto& v : m_varNames) varChoices.Add(wxString::FromUTF8(v));
    m_varCtrl = new wxChoice(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, varChoices);
    if (initialData.contains("variable") && initialData["variable"].is_string()) {
        m_varCtrl->SetStringSelection(
            wxString::FromUTF8(initialData["variable"].get<std::string>()));
    } else if (!m_varNames.empty()) {
        m_varCtrl->SetSelection(0);
    }
    grid->Add(m_varCtrl, 1, wxEXPAND);

    // Target Value
    m_valueLabel = new wxStaticText(card, wxID_ANY, _("Target Value (Write):"));
    m_valueLabel->SetForegroundColour(ui::kTextColor);
    grid->Add(m_valueLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_valueCtrl =
        new wxTextCtrl(card, wxID_ANY, wxString::FromUTF8(initialData.value("value", "")));
    grid->Add(m_valueCtrl, 1, wxEXPAND);

    // Wait
    m_waitLabel = new wxStaticText(card, wxID_ANY, _("Wait (ms):"));
    m_waitLabel->SetForegroundColour(ui::kTextColor);
    grid->Add(m_waitLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_waitCtrl = new wxSpinCtrl(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 60000, initialData.value("wait_ms", 100));
    grid->Add(m_waitCtrl, 1, wxEXPAND);

    // Assert Type
    m_assertTypeLabel = new wxStaticText(card, wxID_ANY, _("Assert Type:"));
    m_assertTypeLabel->SetForegroundColour(ui::kTextColor);
    grid->Add(m_assertTypeLabel, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString asserts;
    asserts.Add("");       // 无断言
    asserts.Add("==");     // 等于
    asserts.Add("!=");     // 不等于
    asserts.Add(">");      // 大于
    asserts.Add("<");      // 小于
    asserts.Add("regex");  // 正则
    m_assertTypeCtrl = new wxChoice(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, asserts);
    if (initialData.contains("assert") && initialData["assert"].contains("type")) {
        std::string t = initialData["assert"]["type"].get<std::string>();
        // 兼容旧版 eq/gt/lt
        if (t == "eq")
            t = "==";
        else if (t == "gt")
            t = ">";
        else if (t == "lt")
            t = "<";
        m_assertTypeCtrl->SetStringSelection(wxString::FromUTF8(t));
    } else {
        m_assertTypeCtrl->SetSelection(0);
    }
    grid->Add(m_assertTypeCtrl, 1, wxEXPAND);

    // Assert Expected
    m_assertExpectedLabel = new wxStaticText(card, wxID_ANY, _("Assert Expected:"));
    m_assertExpectedLabel->SetForegroundColour(ui::kTextColor);
    grid->Add(m_assertExpectedLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_assertExpectedCtrl = new wxTextCtrl(card, wxID_ANY, "");
    if (initialData.contains("assert") && initialData["assert"].contains("expected")) {
        m_assertExpectedCtrl->SetValue(
            wxString::FromUTF8(initialData["assert"]["expected"].get<std::string>()));
    }
    grid->Add(m_assertExpectedCtrl, 1, wxEXPAND);

    cardSizer->Add(grid, 1, wxEXPAND | wxALL, 16);
    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // 3. 底部扁平按钮
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnCancel = new ui::FlatButton(this, _("Cancel"), ui::kBtnGrayBg, ui::kTextColor,
                                         [this]() { EndModal(wxID_CANCEL); });
    auto* btnOk = new ui::FlatButton(this, _("OK"), ui::kThemeBlue, *wxWHITE,
                                     [this]() { OnOk(); });

    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->Add(btnOk, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

    SetSizerAndFit(mainSizer);
    SetMinSize(FromDIP(wxSize(450, 400)));
    CentreOnParent();

    // 初始化显隐状态
    UpdateUiVisibility();
}

void TestStepEditDialog::OnActionChanged(wxCommandEvent&) {
    UpdateUiVisibility();
}

void TestStepEditDialog::UpdateUiVisibility() {
    std::string action = std::string(m_actionCtrl->GetStringSelection().utf8_str());
    bool isRead = (action == "read");
    bool isWrite = (action == "write");
    bool isDelay = (action == "delay");

    // Variable 只有在非 delay 时可见
    m_varLabel->Show(!isDelay);
    m_varCtrl->Show(!isDelay);

    // Value 只有在 write 时可见
    m_valueLabel->Show(isWrite);
    m_valueCtrl->Show(isWrite);

    // 断言只有在 read 时可见
    m_assertTypeLabel->Show(isRead);
    m_assertTypeCtrl->Show(isRead);
    m_assertExpectedLabel->Show(isRead);
    m_assertExpectedCtrl->Show(isRead);

    // 重新排版
    GetSizer()->Layout();
    Layout();
    Fit();
}

void TestStepEditDialog::OnOk() {
    const std::string action = std::string(m_actionCtrl->GetStringSelection().utf8_str());
    if (action == "write" && m_valueCtrl->IsShown()) {
        const std::string value = std::string(m_valueCtrl->GetValue().utf8_str());
        std::string varName;
        if (m_varCtrl->GetSelection() != wxNOT_FOUND)
            varName = std::string(m_varCtrl->GetStringSelection().utf8_str());

        auto it = m_varTypes.find(varName);
        if (!value.empty() && it != m_varTypes.end()) {
            std::string error;
            if (!ValueValidator::Validate(it->second, value, error)) {
                wxMessageBox(wxString::Format(
                                 _("Invalid value for variable '%s' (type %s):\n\n%s"),
                                 wxString::FromUTF8(varName), wxString::FromUTF8(it->second),
                                 wxString::FromUTF8(error)),
                             _("Invalid Value"), wxOK | wxICON_WARNING, this);
                m_valueCtrl->SetFocus();
                return;
            }
        }
    }
    EndModal(wxID_OK);
}

nlohmann::json TestStepEditDialog::GetStepJson() const {
    nlohmann::json step;
    step["name"] = std::string(m_nameCtrl->GetValue().utf8_str());
    step["action"] = std::string(m_actionCtrl->GetStringSelection().utf8_str());

    if (m_varCtrl->IsShown() && m_varCtrl->GetSelection() != wxNOT_FOUND) {
        step["variable"] = std::string(m_varCtrl->GetStringSelection().utf8_str());
    }

    if (m_valueCtrl->IsShown() && !m_valueCtrl->GetValue().IsEmpty()) {
        step["value"] = std::string(m_valueCtrl->GetValue().utf8_str());
    }

    step["wait_ms"] = m_waitCtrl->GetValue();

    if (m_assertTypeCtrl->IsShown() && m_assertTypeCtrl->GetSelection() > 0) {
        step["assert"] = {{"type", std::string(m_assertTypeCtrl->GetStringSelection().utf8_str())},
                          {"expected", std::string(m_assertExpectedCtrl->GetValue().utf8_str())}};
    }

    return step;
}

}  // namespace icu
