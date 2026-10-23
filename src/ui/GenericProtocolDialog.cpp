#include "GenericProtocolDialog.h"

#include <wx/spinctrl.h>
#include <wx/statline.h>

namespace icu {

// clang-format off
wxBEGIN_EVENT_TABLE(GenericProtocolDialog, wxDialog)
    EVT_RADIOBOX(wxID_ANY, GenericProtocolDialog::OnModeChanged)
wxEND_EVENT_TABLE();
// clang-format on

GenericProtocolDialog::GenericProtocolDialog(wxWindow* parent, IProtocol* protocol,
                                             int initialModeIndex,
                                             const std::map<std::string, FieldValue>& initialValues)
    : wxDialog(parent, wxID_ANY, _("New Connection"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_protocol(protocol),
      m_initialMode(initialModeIndex),
      m_initialValues(initialValues) {
    BuildUi();
}

void GenericProtocolDialog::BuildUi() {
    // 开启双缓冲，防止严重闪烁和绘制卡顿
    SetDoubleBuffered(true);

    // 设置一个合适的初始最小宽度
    SetMinSize(FromDIP(wxSize(420, -1)));

    m_mainSizer = new wxBoxSizer(wxVERTICAL);

    if (m_protocol->HasModes()) {
        auto names = m_protocol->ModeNames();
        wxArrayString modeArr;
        for (auto& n : names) modeArr.Add(wxGetTranslation(wxString::FromUTF8(n)));

        m_modeRadio = new wxRadioBox(this, wxID_ANY, _("Mode"), wxDefaultPosition, wxDefaultSize,
                                     modeArr, 1, wxRA_SPECIFY_COLS);
        const int sel =
            (m_initialMode >= 0 && m_initialMode < (int)names.size()) ? m_initialMode : 0;
        m_modeRadio->SetSelection(sel);
        m_mainSizer->Add(m_modeRadio, 0, wxEXPAND | wxALL, FromDIP(10));
    }

    // 动态内容区域
    m_dynamicSizer = new wxBoxSizer(wxVERTICAL);
    m_mainSizer->Add(m_dynamicSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    m_mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxALL, FromDIP(10));

    // ★ 统一按钮区域：靠右对齐，且留出合适的边距
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();

    auto* btnOk = new wxButton(this, wxID_OK, _("OK"));
    auto* btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
    btnOk->SetDefault();

    btnSizer->Add(btnOk, 0, wxRIGHT, FromDIP(10));
    btnSizer->Add(btnCancel, 0);

    m_mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    SetSizer(m_mainSizer);

    const int modeCount = m_protocol->HasModes() ? (int)m_protocol->ModeNames().size() : 1;
    m_controlsPerMode.resize(modeCount);

    // 初始只生成当前选中模式的面板
    m_activeModeIndex =
        m_protocol->HasModes() ? (m_modeRadio ? m_modeRadio->GetSelection() : 0) : 0;
    RebuildFieldsPanel(m_activeModeIndex);
}

// 动态构建/切换面板（按需加载，解决卡顿）
void GenericProtocolDialog::RebuildFieldsPanel(int modeIndex) {
    if (modeIndex == m_activeModeIndex && m_currentPanel != nullptr) return;

    Freeze();  // 冻结绘制

    if (m_currentPanel) {
        m_dynamicSizer->Detach(m_currentPanel);
        m_currentPanel->Destroy();
        m_currentPanel = nullptr;
    }

    auto fields = m_protocol->FieldsForMode(modeIndex);
    m_controlsPerMode[modeIndex].clear();

    m_currentPanel = CreateFieldsPanel(this, fields, m_controlsPerMode[modeIndex]);
    m_dynamicSizer->Add(m_currentPanel, 1, wxEXPAND);

    ApplyInitialValues(modeIndex, m_controlsPerMode[modeIndex]);

    m_activeModeIndex = modeIndex;

    // 重新计算对话框的最佳尺寸，避免被遮挡
    Layout();
    Fit();
    SetMinSize(GetSize());  // 锁死当前大小，防止用户能拖得特别小导致控件被遮挡

    // 如果窗口居中了，重新居中
    CentreOnParent();

    Thaw();  // 恢复绘制
}

wxPanel* GenericProtocolDialog::CreateFieldsPanel(wxWindow* parent,
                                                  const std::vector<FieldDescriptor>& fields,
                                                  std::map<std::string, wxWindow*>& outControls) {
    auto* panel = new wxPanel(parent);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(8));
    grid->AddGrowableCol(1, 1);

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

    if (grid->GetItemCount() > 0) {
        mainSizer->Add(grid, 0, wxEXPAND | wxALL, FromDIP(8));
    }

    for (const auto& pair : groupedFields) {
        const std::string& groupName = pair.first;
        const auto& groupFds = pair.second;

        auto* box = new wxStaticBoxSizer(wxVERTICAL, panel,
                                         wxGetTranslation(wxString::FromUTF8(groupName)));
        auto* boxGrid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(8));
        boxGrid->AddGrowableCol(1, 1);

        for (const auto* fd : groupFds) {
            boxGrid->Add(new wxStaticText(box->GetStaticBox(), wxID_ANY,
                                          wxGetTranslation(wxString::FromUTF8(fd->label))),
                         0, wxALIGN_CENTER_VERTICAL);
            wxWindow* ctrl = CreateControl(box->GetStaticBox(), *fd);
            boxGrid->Add(ctrl, 1, wxEXPAND);
            outControls[fd->key] = ctrl;
        }

        box->Add(boxGrid, 1, wxEXPAND | wxALL, FromDIP(8));
        mainSizer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
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

void GenericProtocolDialog::ApplyInitialValues(int modeIndex,
                                               std::map<std::string, wxWindow*>& controls) {
    if (m_initialValues.empty()) return;

    auto fields = m_protocol->FieldsForMode(modeIndex);

    for (const auto& fd : fields) {
        auto vit = m_initialValues.find(fd.key);
        if (vit == m_initialValues.end()) continue;

        auto cit = controls.find(fd.key);
        if (cit == controls.end()) continue;

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
    int newMode = m_modeRadio->GetSelection();
    RebuildFieldsPanel(newMode);
}

void GenericProtocolDialog::CollectValues(int modeIndex,
                                          const std::map<std::string, wxWindow*>& controls,
                                          std::map<std::string, FieldValue>& out) const {
    auto fields = m_protocol->FieldsForMode(modeIndex);

    for (const auto& fd : fields) {
        auto it = controls.find(fd.key);
        if (it == controls.end()) continue;
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
    return m_modeRadio ? m_modeRadio->GetSelection() : 0;
}

std::map<std::string, FieldValue> GenericProtocolDialog::GetValues() const {
    std::map<std::string, FieldValue> out;
    int modeIdx = GetModeIndex();

    if (modeIdx >= 0 && modeIdx < (int)m_controlsPerMode.size()) {
        CollectValues(modeIdx, m_controlsPerMode[modeIdx], out);
    }
    return out;
}

}  // namespace icu
