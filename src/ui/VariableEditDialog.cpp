#include "VariableEditDialog.h"

#include <wx/statbox.h>
#include <wx/stattext.h>

#include "ui/UIUtils.h"

namespace icu {

VariableEditDialog::VariableEditDialog(wxWindow* parent, IProtocol* protocol,
                                       const std::string& varName, const nlohmann::json& varData,
                                       bool isEditMode)
    : wxDialog(parent, wxID_ANY, isEditMode ? _("Edit Variable") : _("Add Variable"),
               wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxRESIZE_BORDER),
      m_isEditMode(isEditMode) {
    SetBackgroundColour(ui::kBgColor);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. 标题栏
    auto* header =
        ui::CreateDialogHeader(this, isEditMode ? _("Edit Variable") : _("Add Variable"),
                               m_dragStart, m_winStart, [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    // 2. 主体卡片
    auto* card = new ui::CardPanel(this);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    // --- 基础信息区 ---
    auto* baseGrid = new wxFlexGridSizer(2, 8, 12);
    baseGrid->AddGrowableCol(1, 1);

    auto* lblName = new wxStaticText(card, wxID_ANY, _("Variable Name:"));
    lblName->SetForegroundColour(ui::kTextColor);
    baseGrid->Add(lblName, 0, wxALIGN_CENTER_VERTICAL);
    // 允许编辑名称，使用原生样式但置于白卡上，不会显得突兀
    m_nameCtrl = new wxTextCtrl(card, wxID_ANY, wxString::FromUTF8(varName));
    baseGrid->Add(m_nameCtrl, 1, wxEXPAND);

    auto* lblType = new wxStaticText(card, wxID_ANY, _("Data Type:"));
    lblType->SetForegroundColour(ui::kTextColor);
    baseGrid->Add(lblType, 0, wxALIGN_CENTER_VERTICAL);
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

    m_typeCtrl = new wxChoice(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, types);
    m_typeCtrl->SetStringSelection(wxString::FromUTF8(varData.value("type", "int32")));
    baseGrid->Add(m_typeCtrl, 1, wxEXPAND);

    auto* lblAccess = new wxStaticText(card, wxID_ANY, _("Access:"));
    lblAccess->SetForegroundColour(ui::kTextColor);
    baseGrid->Add(lblAccess, 0, wxALIGN_CENTER_VERTICAL);
    wxArrayString accesses;
    accesses.Add("RW");
    accesses.Add("RO");
    accesses.Add("WO");
    m_accessCtrl = new wxChoice(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, accesses);
    m_accessCtrl->SetStringSelection(wxString::FromUTF8(varData.value("access", "RW")));
    baseGrid->Add(m_accessCtrl, 1, wxEXPAND);

    cardSizer->Add(baseGrid, 0, wxEXPAND | wxALL, 16);

    // --- 动态协议参数区 ---
    if (protocol) {
        auto fields = protocol->VariableParamsFields();
        if (!fields.empty()) {
            // 使用浅灰底色块代表参数区，去掉生硬的 StaticBox 边框
            auto* paramBox =
                new wxPanel(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
            paramBox->SetBackgroundColour(wxColour(249, 250, 251));
            auto* pBoxSizer = new wxBoxSizer(wxVERTICAL);

            auto* pTitle = new wxStaticText(paramBox, wxID_ANY, _("Protocol Parameters"));
            wxFont ptf = pTitle->GetFont();
            ptf.MakeBold();
            pTitle->SetFont(ptf);
            pTitle->SetForegroundColour(ui::kThemeDark);
            pBoxSizer->Add(pTitle, 0, wxALL, 10);

            auto* pGrid = new wxFlexGridSizer(2, 8, 12);
            pGrid->AddGrowableCol(1, 1);

            nlohmann::json currentParams =
                varData.contains("params") ? varData["params"] : nlohmann::json::object();

            for (const auto& fd : fields) {
                auto* lbl = new wxStaticText(paramBox, wxID_ANY,
                                             wxGetTranslation(wxString::FromUTF8(fd.label)));
                lbl->SetForegroundColour(ui::kTextColor);
                pGrid->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);

                nlohmann::json val =
                    currentParams.contains(fd.key) ? currentParams[fd.key] : nlohmann::json();
                wxWindow* ctrl = CreateParamControl(paramBox, fd, val);
                pGrid->Add(ctrl, 1, wxEXPAND);
                m_paramControls[fd.key] = ctrl;
            }

            pBoxSizer->Add(pGrid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
            paramBox->SetSizer(pBoxSizer);

            cardSizer->Add(paramBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        }
    }

    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // 3. 底部扁平按钮
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnCancel = new ui::FlatButton(this, _("Cancel"), ui::kBtnGrayBg, ui::kTextColor,
                                         [this]() { EndModal(wxID_CANCEL); });
    auto* btnOk = new ui::FlatButton(this, _("OK"), ui::kThemeBlue, *wxWHITE,
                                     [this]() { EndModal(wxID_OK); });

    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->Add(btnOk, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

    SetSizerAndFit(mainSizer);
    SetMinSize(FromDIP(wxSize(400, 350)));
    CentreOnParent();
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
                    params[key] = v;
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
