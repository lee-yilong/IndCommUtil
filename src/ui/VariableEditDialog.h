#pragma once

#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "core/IProtocol.h"

namespace icu {

class VariableEditDialog : public wxDialog {
public:
    VariableEditDialog(wxWindow* parent, IProtocol* protocol, const std::string& varName,
                       const nlohmann::json& varData, bool isEditMode);
    ~VariableEditDialog() override = default;

    std::string GetVarName() const;
    nlohmann::json GetVarJson() const;

private:
    wxTextCtrl* m_nameCtrl = nullptr;
    wxChoice* m_typeCtrl = nullptr;
    wxChoice* m_accessCtrl = nullptr;

    // 动态参数控件表
    std::map<std::string, wxWindow*> m_paramControls;
    bool m_isEditMode;

    wxWindow* CreateParamControl(wxWindow* parent, const FieldDescriptor& fd,
                                 const nlohmann::json& currentVal);

    // 拖拽用
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
