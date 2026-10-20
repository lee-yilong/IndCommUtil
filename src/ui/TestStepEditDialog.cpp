#include "TestStepEditDialog.h"

#include <wx/stattext.h>

namespace icu {

TestStepEditDialog::TestStepEditDialog(wxWindow* parent, const nlohmann::json& initialData,
                                       const std::vector<std::string>& availableVars)
    : wxDialog(parent, wxID_ANY, _("Edit Test Step"), wxDefaultPosition, wxSize(400, -1)) {
    m_varNames = availableVars;

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Step Name:")), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl =
        new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initialData.value("name", "New Step")));
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Action:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString actions;
    actions.Add("read");
    actions.Add("write");
    actions.Add("delay");
    m_actionCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, actions);

    std::string initAction = initialData.value("action", "read");
    if (initAction == "send") initAction = "write";  // 兼容旧数据
    m_actionCtrl->SetStringSelection(wxString::FromUTF8(initAction));
    if (m_actionCtrl->GetSelection() == wxNOT_FOUND) m_actionCtrl->SetSelection(0);
    m_actionCtrl->Bind(wxEVT_CHOICE, &TestStepEditDialog::OnActionChanged, this);
    grid->Add(m_actionCtrl, 1, wxEXPAND);

    m_varLabel = new wxStaticText(this, wxID_ANY, _("Variable:"));
    grid->Add(m_varLabel, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString varChoices;
    for (const auto& v : m_varNames) varChoices.Add(wxString::FromUTF8(v));
    m_varCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, varChoices);
    if (initialData.contains("variable") && initialData["variable"].is_string()) {
        m_varCtrl->SetStringSelection(
            wxString::FromUTF8(initialData["variable"].get<std::string>()));
    } else if (!m_varNames.empty()) {
        m_varCtrl->SetSelection(0);
    }
    grid->Add(m_varCtrl, 1, wxEXPAND);

    m_valueLabel = new wxStaticText(this, wxID_ANY, _("Target Value (Write):"));
    grid->Add(m_valueLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_valueCtrl =
        new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initialData.value("value", "")));
    grid->Add(m_valueCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, _("Wait (ms):")), 0, wxALIGN_CENTER_VERTICAL);
    m_waitCtrl = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 60000, initialData.value("wait_ms", 100));
    grid->Add(m_waitCtrl, 1, wxEXPAND);

    m_assertTypeLabel = new wxStaticText(this, wxID_ANY, _("Assert Type:"));
    grid->Add(m_assertTypeLabel, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString asserts;
    asserts.Add("");       // 无断言
    asserts.Add("==");     // 等于
    asserts.Add("!=");     // 不等于
    asserts.Add(">");      // 大于
    asserts.Add("<");      // 小于
    asserts.Add("regex");  // 正则
    m_assertTypeCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, asserts);
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

    m_assertExpectedLabel = new wxStaticText(this, wxID_ANY, _("Assert Expected:"));
    grid->Add(m_assertExpectedLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_assertExpectedCtrl = new wxTextCtrl(this, wxID_ANY, "");
    if (initialData.contains("assert") && initialData["assert"].contains("expected")) {
        m_assertExpectedCtrl->SetValue(
            wxString::FromUTF8(initialData["assert"]["expected"].get<std::string>()));
    }
    grid->Add(m_assertExpectedCtrl, 1, wxEXPAND);

    mainSizer->Add(grid, 1, wxEXPAND | wxALL, 15);

    if (auto* btns = CreateButtonSizer(wxOK | wxCANCEL)) {
        mainSizer->Add(btns, 0, wxEXPAND | wxALL, 10);
    }

    SetSizerAndFit(mainSizer);

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

    // 断言只有在 read 时可见（或者你认为 write 也可以断言，可改为 !isDelay）
    // 通常工业上 read 需要验证返回值，write 成功即可
    m_assertTypeLabel->Show(isRead);
    m_assertTypeCtrl->Show(isRead);
    m_assertExpectedLabel->Show(isRead);
    m_assertExpectedCtrl->Show(isRead);

    Layout();
    Fit();
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
