#pragma once

#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace icu {

class TestStepEditDialog : public wxDialog {
public:
    TestStepEditDialog(wxWindow* parent, const nlohmann::json& initialData,
                       const std::vector<std::string>& availableVars);
    ~TestStepEditDialog() override = default;

    nlohmann::json GetStepJson() const;

private:
    wxTextCtrl* m_nameCtrl = nullptr;
    wxChoice* m_actionCtrl = nullptr;

    wxStaticText* m_varLabel = nullptr;
    wxChoice* m_varCtrl = nullptr;

    wxStaticText* m_valueLabel = nullptr;
    wxTextCtrl* m_valueCtrl = nullptr;

    wxStaticText* m_waitLabel = nullptr;
    wxSpinCtrl* m_waitCtrl = nullptr;

    wxStaticText* m_assertTypeLabel = nullptr;
    wxChoice* m_assertTypeCtrl = nullptr;

    wxStaticText* m_assertExpectedLabel = nullptr;
    wxTextCtrl* m_assertExpectedCtrl = nullptr;

    std::vector<std::string> m_varNames;

    void OnActionChanged(wxCommandEvent& evt);
    void UpdateUiVisibility();

    // 拖拽用
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
