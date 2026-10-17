#include "GenericProtocolDialog.h"

#include <wx/spinctrl.h>

namespace icu {

// clang-format off
// wxWidgets 事件表宏展开后是一条语句，clang-format 会把后续行的缩进逐级累积
wxBEGIN_EVENT_TABLE(GenericProtocolDialog, wxDialog)
    EVT_RADIOBOX(wxID_ANY, GenericProtocolDialog::OnModeChanged) wxEND_EVENT_TABLE();
// clang-format on

GenericProtocolDialog::GenericProtocolDialog(wxWindow* parent, IProtocol* protocol,
                                             int initialModeIndex,
                                             const std::map<std::string, FieldValue>& initialValues)
    : wxDialog(parent, wxID_ANY, _("New Connection"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE),
      m_protocol(protocol),
      m_initialMode(initialModeIndex),
      m_initialValues(initialValues) {
    BuildUi();
}

void GenericProtocolDialog::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);

    if (m_protocol->HasModes()) {
        auto names = m_protocol->ModeNames();
        wxArrayString modeArr;
        for (auto& n : names) modeArr.Add(wxGetTranslation(wxString::FromUTF8(n)));

        m_modeRadio = new wxRadioBox(this, wxID_ANY, _("Mode"), wxDefaultPosition, wxDefaultSize,
                                     modeArr, 1, wxRA_SPECIFY_COLS);
        const int sel =
            (m_initialMode >= 0 && m_initialMode < (int)names.size()) ? m_initialMode : 0;
        m_modeRadio->SetSelection(sel);
        main->Add(m_modeRadio, 0, wxEXPAND | wxALL, 10);
    }

    const int modeCount = m_protocol->HasModes() ? (int)m_protocol->ModeNames().size() : 1;

    m_modePanels.resize(modeCount);
    m_controlsPerMode.resize(modeCount);

    for (int i = 0; i < modeCount; ++i) {
        auto fields = m_protocol->FieldsForMode(i);
        auto* panel = BuildFieldsPanel(this, fields, m_controlsPerMode[i]);
        m_modePanels[i] = panel;
        main->Add(panel, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    }

    const int activeMode = m_protocol->HasModes() ? m_initialMode : 0;
    for (int i = 0; i < modeCount; ++i) m_modePanels[i]->Show(i == activeMode);

    ApplyInitialValues(activeMode);

    if (auto* btns = CreateButtonSizer(wxOK | wxCANCEL)) main->Add(btns, 0, wxEXPAND | wxALL, 10);

    SetSizerAndFit(main);
    Centre();
}

wxPanel* GenericProtocolDialog::BuildFieldsPanel(wxWindow* parent,
                                                 const std::vector<FieldDescriptor>& fields,
                                                 std::map<std::string, wxWindow*>& outControls) {
    auto* panel = new wxPanel(parent);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 主网格（平铺字段）
    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    // 用于收集分组字段
    std::map<std::string, std::vector<const FieldDescriptor*>> groupedFields;

    for (const auto& fd : fields) {
        if (fd.group.empty()) {
            grid->Add(
                new wxStaticText(panel, wxID_ANY, wxGetTranslation(wxString::FromUTF8(fd.label))),
                0, wxALIGN_CENTER_VERTICAL);
            wxWindow* ctrl = CreateControl(panel, fd);
            grid->Add(ctrl, 1, wxEXPAND);
            outControls[fd.key] = ctrl;
        } else {
            groupedFields[fd.group].push_back(&fd);
        }
    }

    mainSizer->Add(grid, 0, wxEXPAND | wxALL, 8);

    // 渲染带边框的分组
    for (const auto& pair : groupedFields) {
        const std::string& groupName = pair.first;
        const auto& groupFds = pair.second;

        auto* box = new wxStaticBoxSizer(wxVERTICAL, panel,
                                         wxGetTranslation(wxString::FromUTF8(groupName)));
        auto* boxGrid = new wxFlexGridSizer(2, 8, 8);
        boxGrid->AddGrowableCol(1, 1);

        for (const auto* fd : groupFds) {
            boxGrid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY,
                                          wxGetTranslation(wxString::FromUTF8(fd->label))),
                         0, wxALIGN_CENTER_VERTICAL);
            wxWindow* ctrl = CreateControl(box->GetStaticBox(), *fd);
            boxGrid->Add(ctrl, 1, wxEXPAND);
            outControls[fd->key] = ctrl;
        }

        box->Add(boxGrid, 1, wxEXPAND | wxALL, 8);
        mainSizer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    }

    panel->SetSizer(mainSizer);
    return panel;
}

wxWindow* GenericProtocolDialog::CreateControl(wxWindow* parent, const FieldDescriptor& fd) {
    switch (fd.type) {
        case FieldValue::Type::String: {
            auto* t =
                new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(fd.defaultValue.AsString()));
            return t;
        }
        case FieldValue::Type::Int: {
            auto* s =
                new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                               wxSP_ARROW_KEYS, fd.minValue, fd.maxValue, fd.defaultValue.AsInt());
            return s;
        }
        case FieldValue::Type::Bool: {
            auto* c = new wxCheckBox(parent, wxID_ANY, "");
            c->SetValue(fd.defaultValue.AsBool());
            return c;
        }
        case FieldValue::Type::Choice: {
            wxArrayString labels;
            int sel = 0;
            for (size_t i = 0; i < fd.choices.size(); ++i) {
                labels.Add(wxGetTranslation(wxString::FromUTF8(fd.choices[i].second)));
                if (fd.choices[i].first == fd.defaultValue.AsString()) sel = (int)i;
            }
            auto* ch = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, labels);
            ch->SetSelection(sel);
            return ch;
        }
    }
    return new wxTextCtrl(parent, wxID_ANY);
}

void GenericProtocolDialog::ApplyInitialValues(int modeIndex) {
    if (m_initialValues.empty()) return;
    if (modeIndex < 0 || modeIndex >= (int)m_controlsPerMode.size()) return;

    auto fields = m_protocol->FieldsForMode(modeIndex);
    const auto& ctrls = m_controlsPerMode[modeIndex];

    for (const auto& fd : fields) {
        auto vit = m_initialValues.find(fd.key);
        if (vit == m_initialValues.end()) continue;

        auto cit = ctrls.find(fd.key);
        if (cit == ctrls.end()) continue;

        const FieldValue& v = vit->second;
        wxWindow* w = cit->second;

        switch (fd.type) {
            case FieldValue::Type::String:
                static_cast<wxTextCtrl*>(w)->SetValue(wxString::FromUTF8(v.AsString()));
                break;
            case FieldValue::Type::Int:
                static_cast<wxSpinCtrl*>(w)->SetValue(v.AsInt());
                break;
            case FieldValue::Type::Bool:
                static_cast<wxCheckBox*>(w)->SetValue(v.AsBool());
                break;
            case FieldValue::Type::Choice: {
                auto* ch = static_cast<wxChoice*>(w);
                const std::string& target = v.AsString();
                for (size_t i = 0; i < fd.choices.size(); ++i) {
                    if (fd.choices[i].first == target) {
                        ch->SetSelection((int)i);
                        break;
                    }
                }
                break;
            }
        }
    }
}

void GenericProtocolDialog::OnModeChanged(wxCommandEvent&) {
    if (!m_protocol->HasModes()) return;

    const int newMode = m_modeRadio->GetSelection();
    for (size_t i = 0; i < m_modePanels.size(); ++i) m_modePanels[i]->Show((int)i == newMode);

    GetSizer()->Layout();
    Fit();
    Layout();
}

void GenericProtocolDialog::CollectValues(int modeIndex,
                                          std::map<std::string, FieldValue>& out) const {
    if (modeIndex < 0 || modeIndex >= (int)m_controlsPerMode.size()) return;

    auto fields = m_protocol->FieldsForMode(modeIndex);
    const auto& ctrls = m_controlsPerMode[modeIndex];

    for (const auto& fd : fields) {
        auto it = ctrls.find(fd.key);
        if (it == ctrls.end()) continue;
        wxWindow* w = it->second;

        switch (fd.type) {
            case FieldValue::Type::String:
                out[fd.key] = std::string(static_cast<wxTextCtrl*>(w)->GetValue().utf8_str());
                break;
            case FieldValue::Type::Int:
                out[fd.key] = static_cast<wxSpinCtrl*>(w)->GetValue();
                break;
            case FieldValue::Type::Bool:
                out[fd.key] = static_cast<wxCheckBox*>(w)->GetValue();
                break;
            case FieldValue::Type::Choice: {
                auto* ch = static_cast<wxChoice*>(w);
                const int idx = ch->GetSelection();
                if (idx >= 0 && idx < (int)fd.choices.size()) out[fd.key] = fd.choices[idx].first;
                break;
            }
        }
    }
}

int GenericProtocolDialog::GetModeIndex() const {
    if (!m_protocol->HasModes()) return 0;
    return m_modeRadio->GetSelection();
}

std::map<std::string, FieldValue> GenericProtocolDialog::GetValues() const {
    std::map<std::string, FieldValue> out;
    CollectValues(GetModeIndex(), out);
    return out;
}

}  // namespace icu
