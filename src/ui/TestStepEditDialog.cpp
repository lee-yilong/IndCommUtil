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

    // Name
    grid->Add(new wxStaticText(this, wxID_ANY, _("Step Name:")), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl =
        new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initialData.value("name", "New Step")));
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    // Action
    grid->Add(new wxStaticText(this, wxID_ANY, _("Action:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString actions;
    actions.Add("read");
    actions.Add("write");
    actions.Add("send");
    actions.Add("delay");
    m_actionCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, actions);
    m_actionCtrl->SetStringSelection(wxString::FromUTF8(initialData.value("action", "read")));
    if (m_actionCtrl->GetSelection() == wxNOT_FOUND) m_actionCtrl->SetSelection(0);
    grid->Add(m_actionCtrl, 1, wxEXPAND);

    // Variable
    grid->Add(new wxStaticText(this, wxID_ANY, _("Variable:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString varChoices;
    for (const auto& v : m_varNames) varChoices.Add(wxString::FromUTF8(v));
    m_varCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, varChoices);
    if (initialData.contains("variable") && initialData["variable"].is_string()) {
        m_varCtrl->SetStringSelection(
            wxString::FromUTF8(initialData["variable"].get<std::string>()));
    }
    grid->Add(m_varCtrl, 1, wxEXPAND);

    // Target Value (For Write)
    grid->Add(new wxStaticText(this, wxID_ANY, _("Target Value (Write):")), 0,
              wxALIGN_CENTER_VERTICAL);
    m_valueCtrl =
        new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initialData.value("value", "")));
    grid->Add(m_valueCtrl, 1, wxEXPAND);

    // Wait MS
    grid->Add(new wxStaticText(this, wxID_ANY, _("Wait (ms):")), 0, wxALIGN_CENTER_VERTICAL);
    m_waitCtrl = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxSP_ARROW_KEYS, 0, 60000, initialData.value("wait_ms", 100));
    grid->Add(m_waitCtrl, 1, wxEXPAND);

    // Assert Type
    grid->Add(new wxStaticText(this, wxID_ANY, _("Assert Type:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString asserts;
    asserts.Add("");
    asserts.Add("eq");
    asserts.Add("regex");
    asserts.Add("gt");
    asserts.Add("lt");
    m_assertTypeCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, asserts);
    if (initialData.contains("assert") && initialData["assert"].contains("type")) {
        m_assertTypeCtrl->SetStringSelection(
            wxString::FromUTF8(initialData["assert"]["type"].get<std::string>()));
    } else {
        m_assertTypeCtrl->SetSelection(0);
    }
    grid->Add(m_assertTypeCtrl, 1, wxEXPAND);

    // Assert Expected
    grid->Add(new wxStaticText(this, wxID_ANY, _("Assert Expected:")), 0, wxALIGN_CENTER_VERTICAL);
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
}

nlohmann::json TestStepEditDialog::GetStepJson() const {
    nlohmann::json step;
    step["name"] = std::string(m_nameCtrl->GetValue().utf8_str());
    step["action"] = std::string(m_actionCtrl->GetStringSelection().utf8_str());

    if (m_varCtrl->GetSelection() != wxNOT_FOUND) {
        step["variable"] = std::string(m_varCtrl->GetStringSelection().utf8_str());
    }

    if (!m_valueCtrl->GetValue().IsEmpty()) {
        step["value"] = std::string(m_valueCtrl->GetValue().utf8_str());
    }

    step["wait_ms"] = m_waitCtrl->GetValue();

    if (m_assertTypeCtrl->GetSelection() > 0) {
        step["assert"] = {{"type", std::string(m_assertTypeCtrl->GetStringSelection().utf8_str())},
                          {"expected", std::string(m_assertExpectedCtrl->GetValue().utf8_str())}};
    }

    return step;
}

}  // namespace icu
