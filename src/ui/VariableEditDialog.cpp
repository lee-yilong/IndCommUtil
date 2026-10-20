#include "VariableEditDialog.h"

#include <wx/statbox.h>
#include <wx/stattext.h>

namespace icu {

VariableEditDialog::VariableEditDialog(wxWindow* parent, IProtocol* protocol,
                                       const std::string& varName, const nlohmann::json& varData,
                                       bool isEditMode)
    : wxDialog(parent, wxID_ANY, _("Edit Variable"), wxDefaultPosition, wxSize(400, -1)),
      m_isEditMode(isEditMode) {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    // Name
    grid->Add(new wxStaticText(this, wxID_ANY, _("Variable Name:")), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(varName));
    if (m_isEditMode) m_nameCtrl->Disable();
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    // Type
    grid->Add(new wxStaticText(this, wxID_ANY, _("Data Type:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString types;
    types.Add("int8");
    types.Add("uint8");
    types.Add("int16");
    types.Add("uint16");
    types.Add("int32");
    types.Add("uint32");
    types.Add("float");
    types.Add("double");
    types.Add("bool");
    types.Add("string");
    types.Add("hex");

    m_typeCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, types);
    m_typeCtrl->SetStringSelection(wxString::FromUTF8(varData.value("type", "int32")));
    grid->Add(m_typeCtrl, 1, wxEXPAND);

    // Access
    grid->Add(new wxStaticText(this, wxID_ANY, _("Access:")), 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString accesses;
    accesses.Add("RW");
    accesses.Add("RO");
    accesses.Add("WO");
    m_accessCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, accesses);
    m_accessCtrl->SetStringSelection(wxString::FromUTF8(varData.value("access", "RW")));
    grid->Add(m_accessCtrl, 1, wxEXPAND);

    mainSizer->Add(grid, 0, wxEXPAND | wxALL, 15);

    // 动态协议参数
    if (protocol) {
        auto fields = protocol->VariableParamsFields();
        if (!fields.empty()) {
            auto* box = new wxStaticBoxSizer(wxVERTICAL, this, _("Protocol Parameters"));
            auto* pGrid = new wxFlexGridSizer(2, 8, 8);
            pGrid->AddGrowableCol(1, 1);

            nlohmann::json currentParams =
                varData.contains("params") ? varData["params"] : nlohmann::json::object();

            for (const auto& fd : fields) {
                pGrid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY,
                                            wxGetTranslation(wxString::FromUTF8(fd.label))),
                           0, wxALIGN_CENTER_VERTICAL);

                nlohmann::json val =
                    currentParams.contains(fd.key) ? currentParams[fd.key] : nlohmann::json();
                wxWindow* ctrl = CreateParamControl(box->GetStaticBox(), fd, val);
                pGrid->Add(ctrl, 1, wxEXPAND);
                m_paramControls[fd.key] = ctrl;
            }
            box->Add(pGrid, 1, wxEXPAND | wxALL, 8);
            mainSizer->Add(box, 1, wxEXPAND | wxLEFT | wxRIGHT, 15);
        }
    }

    if (auto* btns = CreateButtonSizer(wxOK | wxCANCEL)) {
        mainSizer->Add(btns, 0, wxEXPAND | wxALL, 10);
    }
    SetSizerAndFit(mainSizer);
}

wxWindow* VariableEditDialog::CreateParamControl(wxWindow* parent, const FieldDescriptor& fd,
                                                 const nlohmann::json& currentVal) {
    switch (fd.type) {
        case FieldValue::Type::String: {
            wxString defStr = currentVal.is_string()
                                  ? wxString::FromUTF8(currentVal.get<std::string>())
                                  : wxString::FromUTF8(fd.defaultValue.AsString());
            return new wxTextCtrl(parent, wxID_ANY, defStr);
        }
        case FieldValue::Type::Int: {
            int defInt = currentVal.is_number() ? currentVal.get<int>() : fd.defaultValue.AsInt();
            return new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, fd.minValue, fd.maxValue, defInt);
        }
        case FieldValue::Type::Bool: {
            bool defBool =
                currentVal.is_boolean() ? currentVal.get<bool>() : fd.defaultValue.AsBool();
            auto* c = new wxCheckBox(parent, wxID_ANY, "");
            c->SetValue(defBool);
            return c;
        }
        case FieldValue::Type::Choice: {
            wxArrayString labels;
            int sel = 0;
            std::string curChoice =
                currentVal.is_string() ? currentVal.get<std::string>() : fd.defaultValue.AsString();
            for (size_t i = 0; i < fd.choices.size(); ++i) {
                labels.Add(wxGetTranslation(wxString::FromUTF8(fd.choices[i].second)));
                if (fd.choices[i].first == curChoice) sel = (int)i;
            }
            auto* ch = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, labels);
            ch->SetSelection(sel);
            return ch;
        }
    }
    return new wxTextCtrl(parent, wxID_ANY);
}

std::string VariableEditDialog::GetVarName() const {
    return std::string(m_nameCtrl->GetValue().utf8_str());
}

nlohmann::json VariableEditDialog::GetVarJson() const {
    nlohmann::json j;
    j["type"] = std::string(m_typeCtrl->GetStringSelection().utf8_str());
    j["access"] = std::string(m_accessCtrl->GetStringSelection().utf8_str());

    if (!m_paramControls.empty()) {
        nlohmann::json params = nlohmann::json::object();
        for (const auto& [key, ctrl] : m_paramControls) {
            if (auto* t = dynamic_cast<wxTextCtrl*>(ctrl)) {
                params[key] = std::string(t->GetValue().utf8_str());
            } else if (auto* s = dynamic_cast<wxSpinCtrl*>(ctrl)) {
                params[key] = s->GetValue();
            } else if (auto* c = dynamic_cast<wxCheckBox*>(ctrl)) {
                params[key] = c->GetValue();
            } else if (auto* ch = dynamic_cast<wxChoice*>(ctrl)) {
                wxString txt = ch->GetStringSelection();
                long v;
                if (txt.BeforeFirst(' ').ToLong(&v)) {
                    params[key] = v;  // 如果像 Modbus 功能码那样是数字，转回 int
                } else {
                    params[key] = std::string(txt.utf8_str());
                }
            }
        }
        j["params"] = params;
    }
    return j;
}

}  // namespace icu
