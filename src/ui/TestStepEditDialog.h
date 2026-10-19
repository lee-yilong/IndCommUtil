#pragma once

#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "VariableEditDialog.h"

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
    wxChoice* m_varCtrl = nullptr;
    wxTextCtrl* m_valueCtrl = nullptr;
    wxSpinCtrl* m_waitCtrl = nullptr;
    wxChoice* m_assertTypeCtrl = nullptr;
    wxTextCtrl* m_assertExpectedCtrl = nullptr;

    std::vector<std::string> m_varNames;
};

}  // namespace icu
