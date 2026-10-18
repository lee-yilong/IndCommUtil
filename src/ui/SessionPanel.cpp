#include "SessionPanel.h"

#include <wx/datetime.h>
#include <wx/tokenzr.h>

#include <thread>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

class JsonEditDialog : public wxDialog {
public:
    // 增加 readOnlyName 参数
    JsonEditDialog(wxWindow* parent, const wxString& title, const nlohmann::json& initialData,
                   const wxString& varName = "", bool readOnlyName = false)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(400, 350)) {
        auto* sizer = new wxBoxSizer(wxVERTICAL);

        auto* nameSizer = new wxBoxSizer(wxHORIZONTAL);
        nameSizer->Add(new wxStaticText(this, wxID_ANY, _("Variable Name:")), 0,
                       wxALIGN_CENTER_VERTICAL | wxALL, 10);
        m_nameCtrl = new wxTextCtrl(this, wxID_ANY, varName);
        if (readOnlyName) m_nameCtrl->Disable();  // 如果是编辑模式，禁用改名
        nameSizer->Add(m_nameCtrl, 1, wxEXPAND | wxALL, 10);
        sizer->Add(nameSizer, 0, wxEXPAND);

        sizer->Add(new wxStaticText(this, wxID_ANY, _("Variable JSON (type, access, params):")), 0,
                   wxALL, 10);
        m_text = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initialData.dump(4)),
                                wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
        sizer->Add(m_text, 1, wxEXPAND | wxALL, 10);

        if (auto* btns = CreateButtonSizer(wxOK | wxCANCEL))
            sizer->Add(btns, 0, wxEXPAND | wxALL, 10);
        SetSizer(sizer);
    }

    // 重命名，避免和 wxWindow::GetName 冲突
    std::string GetVariableName() const { return std::string(m_nameCtrl->GetValue().utf8_str()); }
    nlohmann::json GetJson() const {
        try {
            return nlohmann::json::parse(std::string(m_text->GetValue().utf8_str()));
        } catch (...) {
            return nlohmann::json::object();
        }
    }

private:
    wxTextCtrl* m_nameCtrl;
    wxTextCtrl* m_text;
};

SessionPanel::SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session,
                           SessionConfig* sessionConfig)
    : wxPanel(parent, wxID_ANY),
      m_protocol(protocol),
      m_modeIndex(modeIndex),
      m_session(session),
      m_sessionConfig(sessionConfig) {
    m_streamSession = dynamic_cast<StreamSession*>(m_session);
    m_txSession = dynamic_cast<ITransactionSession*>(m_session);

    BuildUi();

    if (m_streamSession)
        BindStreamCallbacks();
    else if (m_txSession)
        BindTransactionCallbacks();

    if (m_session) {
        m_lastState = m_session->State();
        UpdateStateLabel(m_lastState);
        UpdateActionButton(m_lastState);
        RefreshFromSession();
    }

    if (m_sessionConfig) {
        if (m_sendTextCtrl) {
            m_sendTextCtrl->SetValue(wxString::FromUTF8(m_sessionConfig->manualMode.sendPayload));
            if (m_appendNewline)
                m_appendNewline->SetValue(m_sessionConfig->manualMode.appendNewline);
        }
        if (m_modeNotebook) {
            m_modeNotebook->SetSelection(static_cast<int>(m_sessionConfig->activeMode));
        }
    }
}

SessionPanel::~SessionPanel() {
    if (m_testRunner) {
        m_testRunner->Stop();
        delete m_testRunner;
    }
}

void SessionPanel::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);
    BuildCommonUi(main);

    m_modeNotebook = new wxNotebook(this, wxID_ANY);
    m_manualPage = new wxPanel(m_modeNotebook);
    m_interactivePage = new wxPanel(m_modeNotebook);
    m_autoPage = new wxPanel(m_modeNotebook);

    m_modeNotebook->AddPage(m_manualPage, _("Manual Mode"));
    m_modeNotebook->AddPage(m_interactivePage, _("Interactive Mode"));
    m_modeNotebook->AddPage(m_autoPage, _("Auto Mode"));
    m_modeNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SessionPanel::OnNotebookPageChanged, this);
    main->Add(m_modeNotebook, 1, wxEXPAND | wxALL, 10);

    // 手动模式页面布局：传入 m_manualPage 作为父窗口
    auto* manualSizer = new wxBoxSizer(wxVERTICAL);
    BuildVariableUi(manualSizer, m_manualPage);
    if (m_streamSession)
        BuildStreamUi(manualSizer, m_manualPage);
    else if (m_txSession)
        BuildTransactionUi(manualSizer, m_manualPage);
    m_manualPage->SetSizer(manualSizer);

    // 交互/自动模式页面布局
    BuildTestFlowUi(m_interactivePage, false);
    BuildTestFlowUi(m_autoPage, true);

    SetSizer(main);
}

void SessionPanel::BuildCommonUi(wxBoxSizer* mainSizer) {
    m_titleLabel = new wxStaticText(this, wxID_ANY, "-");
    wxFont f = m_titleLabel->GetFont();
    f.SetPointSize(f.GetPointSize() + 3);
    f.MakeBold();
    m_titleLabel->SetFont(f);
    m_titleLabel->SetForegroundColour(wxColour(0, 0, 160));
    mainSizer->Add(m_titleLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    auto* header = new wxStaticBoxSizer(wxVERTICAL, this, _("Session Status"));
    m_statusLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, "-");
    wxFont f2 = m_statusLabel->GetFont();
    f2.MakeBold();
    f2.SetPointSize(f2.GetPointSize() + 2);
    m_statusLabel->SetFont(f2);
    header->Add(m_statusLabel, 0, wxALL, 8);
    m_targetLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, "-");
    header->Add(m_targetLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* stats = new wxBoxSizer(wxHORIZONTAL);
    m_rxLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, _("Received: 0 bytes"));
    m_txLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, _("Sent: 0 bytes"));
    m_clientsLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, _("Clients: 0"));
    stats->Add(m_rxLabel, 1, wxALL, 6);
    stats->Add(m_txLabel, 1, wxALL, 6);
    stats->Add(m_clientsLabel, 1, wxALL, 6);
    header->Add(stats, 0, wxEXPAND);
    mainSizer->Add(header, 0, wxEXPAND | wxALL, 10);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_actionBtn = new wxButton(this, wxID_ANY, _("Open Connection"));
    m_actionBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnAction, this);
    btnSizer->Add(m_actionBtn, 0, wxRIGHT, 8);
    m_configBtn = new wxButton(this, wxID_ANY, _("Configure Connection..."));
    m_configBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnConfigure, this);
    btnSizer->Add(m_configBtn, 0);
    mainSizer->Add(btnSizer, 0, wxALL, 10);
}

void SessionPanel::BuildVariableUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    auto* varBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Communication Variables"));

    m_varList = new wxListCtrl(varBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(-1, 150),
                               wxLC_REPORT | wxLC_SINGLE_SEL);
    m_varList->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 150);
    m_varList->InsertColumn(1, _("Type"), wxLIST_FORMAT_LEFT, 100);
    m_varList->InsertColumn(2, _("Access"), wxLIST_FORMAT_LEFT, 80);
    m_varList->InsertColumn(3, _("Value"), wxLIST_FORMAT_LEFT, 150);
    m_varList->Bind(wxEVT_LIST_ITEM_SELECTED, &SessionPanel::OnVarSelected, this);
    varBox->Add(m_varList, 1, wxEXPAND | wxALL, 4);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_addVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Add Variable"));
    m_editVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Edit Variable"));
    m_delVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Delete Variable"));
    m_addVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnAddVariable, this);
    m_editVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnEditVariable, this);
    m_delVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnDeleteVariable, this);

    btnSizer->Add(m_addVarBtn, 0, wxALL, 4);
    btnSizer->Add(m_editVarBtn, 0, wxALL, 4);
    btnSizer->Add(m_delVarBtn, 0, wxALL, 4);
    varBox->Add(btnSizer, 0, wxALIGN_RIGHT);
    mainSizer->Add(varBox, 0, wxEXPAND | wxALL, 10);
    RefreshVariableList();

    m_varOpSizer = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Operation Request"));
    auto* hint = new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                  _("Please select a variable from the list above."));
    m_varOpSizer->Add(hint, 0, wxALL, 10);
    mainSizer->Add(m_varOpSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
}

void SessionPanel::BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    if (m_streamSession->IsServerMode())
        m_clientsLabel->Show();
    else
        m_clientsLabel->Hide();
    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Data Log"));
    auto* logToolbar = new wxBoxSizer(wxHORIZONTAL);
    m_hexDisplay = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("HEX Display"));
    m_showTimestamp = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Timestamp"));
    m_autoScroll = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Auto Scroll"));
    m_clearBtn = new wxButton(logBox->GetStaticBox(), wxID_ANY, _("Clear"));
    m_showTimestamp->SetValue(true);
    m_autoScroll->SetValue(true);
    m_clearBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnClear, this);
    logToolbar->Add(m_hexDisplay, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->Add(m_showTimestamp, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->Add(m_autoScroll, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->AddStretchSpacer();
    logToolbar->Add(m_clearBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logBox->Add(logToolbar, 0, wxEXPAND);

    m_dataLogTextCtrl =
        new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                       wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    logBox->Add(m_dataLogTextCtrl, 1, wxEXPAND | wxALL, 4);
    mainSizer->Add(logBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* sendBox = new wxStaticBoxSizer(wxHORIZONTAL, parentWindow, _("Send"));
    m_sendTextCtrl = new wxTextCtrl(sendBox->GetStaticBox(), wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_sendTextCtrl->Bind(wxEVT_TEXT_ENTER, &SessionPanel::OnSendEnter, this);
    m_sendTextCtrl->Bind(wxEVT_TEXT, &SessionPanel::OnSendTextChanged, this);
    m_sendBtn = new wxButton(sendBox->GetStaticBox(), wxID_ANY, _("Send"));
    m_sendBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnSend, this);
    m_appendNewline = new wxCheckBox(sendBox->GetStaticBox(), wxID_ANY, _("Append newline"));
    m_appendNewline->SetValue(true);
    sendBox->Add(m_sendTextCtrl, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_appendNewline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_sendBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    mainSizer->Add(sendBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
}

void SessionPanel::BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    m_rxLabel->Hide();
    m_txLabel->Hide();
    m_clientsLabel->Hide();
    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Result Log"));
    m_txLogTextCtrl =
        new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                       wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    wxFont mono(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_txLogTextCtrl->SetFont(mono);
    logBox->Add(m_txLogTextCtrl, 1, wxEXPAND | wxALL, 4);
    mainSizer->Add(logBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
}

wxWindow* SessionPanel::CreateControl(wxWindow* parent, const FieldDescriptor& fd) {
    switch (fd.type) {
        case FieldValue::Type::String:
            return new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(fd.defaultValue.AsString()));
        case FieldValue::Type::Int:
            return new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, fd.minValue, fd.maxValue,
                                  fd.defaultValue.AsInt());
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

void SessionPanel::RefreshVariableList() {
    if (!m_varList || !m_sessionConfig) return;
    m_varList->DeleteAllItems();
    int idx = 0;
    for (const auto& [name, var] : m_sessionConfig->variables) {
        long item = m_varList->InsertItem(idx++, wxString::FromUTF8(name));
        m_varList->SetItem(item, 1, wxString::FromUTF8(var.type));
        m_varList->SetItem(item, 2, wxString::FromUTF8(var.access));
        m_varList->SetItem(item, 3, wxString::FromUTF8(var.currentValue));
    }
}

void SessionPanel::OnVarSelected(wxListEvent& evt) {
    long item = evt.GetIndex();
    if (item < 0) return;
    m_selectedVarName = std::string(m_varList->GetItemText(item).utf8_str());
    if (m_sessionConfig && m_sessionConfig->variables.count(m_selectedVarName)) {
        BuildVarOpUi(m_sessionConfig->variables[m_selectedVarName]);
    }
}

void SessionPanel::BuildVarOpUi(const CommVariable& var) {
    m_varOpSizer->Clear(true);
    m_varOpControls.clear();

    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    for (auto& [key, val] : var.params.items()) {
        grid->Add(
            new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY, wxString::FromUTF8(key) + ":"),
            0, wxALIGN_CENTER_VERTICAL);
        wxWindow* ctrl = nullptr;
        if (val.is_number_integer()) {
            ctrl = new wxSpinCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY, "", wxDefaultPosition,
                                  wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535, val.get<int>());
        } else {
            ctrl = new wxTextCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                  wxString::FromUTF8(val.get<std::string>()));
        }
        grid->Add(ctrl, 1, wxEXPAND);
        m_varOpControls[key] = ctrl;
    }

    grid->Add(new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Target Value:")), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* valueCtrl = new wxTextCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                     wxString::FromUTF8(var.currentValue));
    grid->Add(valueCtrl, 1, wxEXPAND);
    m_varOpControls["__value"] = valueCtrl;

    m_varOpSizer->Add(grid, 1, wxEXPAND | wxALL, 8);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_executeReadBtn = new wxButton(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Read"));
    m_executeWriteBtn = new wxButton(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Write"));
    m_executeReadBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecuteRead, this);
    m_executeWriteBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecuteWrite, this);

    if (var.access == "RO") m_executeWriteBtn->Enable(false);
    if (var.access == "WO") m_executeReadBtn->Enable(false);

    btnSizer->Add(m_executeReadBtn, 0, wxALL, 4);
    btnSizer->Add(m_executeWriteBtn, 0, wxALL, 4);
    m_varOpSizer->Add(btnSizer, 0, wxALIGN_RIGHT);

    // ★ 修改：加上强制布局，确保高度撑开
    m_manualPage->Layout();
}

void SessionPanel::OnExecuteRead(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    auto& var = m_sessionConfig->variables[m_selectedVarName];

    for (auto& [key, w] : m_varOpControls) {
        if (key == "__value") continue;
        if (auto* spin = dynamic_cast<wxSpinCtrl*>(w))
            var.params[key] = spin->GetValue();
        else if (auto* txt = dynamic_cast<wxTextCtrl*>(w))
            var.params[key] = std::string(txt->GetValue().utf8_str());
    }

    TransactionRequest req;
    req.slaveId = var.params.value("slaveId", 1);
    req.functionCode = var.params.value("functionCode", 3);
    req.startAddress = var.params.value("startAddress", 0);
    req.count = var.params.value("count", 1);

    AppendTxLog(_("Reading variable: ") + wxString::FromUTF8(m_selectedVarName));
    m_executeReadBtn->Enable(false);

    std::thread([this, req, name = m_selectedVarName]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name]() {
            m_executeReadBtn->Enable(true);
            if (resp.ok && !resp.registers.empty()) {
                std::string valStr = std::to_string(resp.registers[0]);
                m_sessionConfig->variables[name].currentValue = valStr;
                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                    txt->SetValue(valStr);
                for (int i = 0; i < m_varList->GetItemCount(); ++i) {
                    if (m_varList->GetItemText(i) == name) m_varList->SetItem(i, 3, valStr);
                }
                AppendTxLog(wxString::Format("Read Success: %s = %s", wxString::FromUTF8(name),
                                             wxString::FromUTF8(valStr)));
            } else
                AppendTxLog(_("Read failed."));
        });
    }).detach();
}

void SessionPanel::OnExecuteWrite(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    auto& var = m_sessionConfig->variables[m_selectedVarName];

    for (auto& [key, w] : m_varOpControls) {
        if (key == "__value") continue;
        if (auto* spin = dynamic_cast<wxSpinCtrl*>(w))
            var.params[key] = spin->GetValue();
        else if (auto* txt = dynamic_cast<wxTextCtrl*>(w))
            var.params[key] = std::string(txt->GetValue().utf8_str());
    }
    std::string targetVal =
        std::string(dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])->GetValue().utf8_str());

    TransactionRequest req;
    req.slaveId = var.params.value("slaveId", 1);

    // ★ 智能判断：如果当前配置的是读功能码，自动换成写功能码
    int fc = var.params.value("functionCode", 6);
    if (fc == 1)
        fc = 5;  // 读线圈 -> 写单线圈
    else if (fc == 3)
        fc = 6;  // 读保持寄存器 -> 写单寄存器
    else if (fc == 4)
        fc = 6;

    req.functionCode = fc;
    req.startAddress = var.params.value("startAddress", 0);
    try {
        req.writeValues.push_back(std::stoi(targetVal));
    } catch (...) {
    }

    AppendTxLog(_("Writing variable: ") + wxString::FromUTF8(m_selectedVarName) + " = " +
                wxString::FromUTF8(targetVal));
    m_executeWriteBtn->Enable(false);

    std::thread([this, req, name = m_selectedVarName, targetVal]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name, targetVal]() {
            m_executeWriteBtn->Enable(true);
            if (resp.ok) {
                m_sessionConfig->variables[name].currentValue = targetVal;
                for (int i = 0; i < m_varList->GetItemCount(); ++i) {
                    if (m_varList->GetItemText(i) == name) m_varList->SetItem(i, 3, targetVal);
                }
                AppendTxLog(wxString::Format("Write Success: %s = %s", wxString::FromUTF8(name),
                                             wxString::FromUTF8(targetVal)));
            } else
                AppendTxLog(_("Write failed: ") + wxString::FromUTF8(resp.errorMessage.c_str()));
        });
    }).detach();
}

void SessionPanel::OnAddVariable(wxCommandEvent&) {
    if (!m_sessionConfig) return;
    nlohmann::json defaultJson = {
        {"type", "int"},
        {"access", "RW"},
        {"params", {{"slaveId", 1}, {"functionCode", 3}, {"startAddress", 0}, {"count", 1}}}};

    // 默认变量名 "new_var"，允许编辑
    JsonEditDialog dlg(this, _("Add Variable"), defaultJson, "new_var", false);
    if (dlg.ShowModal() == wxID_OK) {
        std::string name = dlg.GetVariableName();
        if (name.empty()) {
            wxMessageBox(_("Variable name cannot be empty."), _("Error"), wxOK | wxICON_ERROR);
            return;
        }
        auto j = dlg.GetJson();
        CommVariable var;
        var.name = name;
        var.type = j.value("type", "int");
        var.access = j.value("access", "RW");
        if (j.contains("params")) var.params = j["params"];

        m_sessionConfig->variables[name] = var;
        RefreshVariableList();
    }
}

void SessionPanel::OnEditVariable(wxCommandEvent&) {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;
    std::string key = std::string(m_varList->GetItemText(sel).utf8_str());
    auto it = m_sessionConfig->variables.find(key);
    if (it == m_sessionConfig->variables.end()) return;

    nlohmann::json editJson = {
        {"type", it->second.type}, {"access", it->second.access}, {"params", it->second.params}};

    // 传入 true，禁用名称输入框
    JsonEditDialog dlg(this, _("Edit Variable"), editJson, wxString::FromUTF8(key), true);

    if (dlg.ShowModal() == wxID_OK) {
        auto j = dlg.GetJson();
        it->second.type = j.value("type", "int");
        it->second.access = j.value("access", "RW");
        if (j.contains("params")) it->second.params = j["params"];
        RefreshVariableList();
    }
}

void SessionPanel::OnDeleteVariable(wxCommandEvent&) {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;
    std::string key = std::string(m_varList->GetItemText(sel).utf8_str());
    m_sessionConfig->variables.erase(key);
    RefreshVariableList();
}

void SessionPanel::OnNotebookPageChanged(wxBookCtrlEvent& evt) {
    int sel = m_modeNotebook->GetSelection();
    if (m_sessionConfig) m_sessionConfig->activeMode = static_cast<SessionMode>(sel);
    evt.Skip();
}

void SessionPanel::BuildTestFlowUi(wxPanel* parent, bool isAutoMode) {
    auto* main = new wxBoxSizer(wxVERTICAL);

    auto* stepList = new wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
    stepList->InsertColumn(0, _("Step Name"), wxLIST_FORMAT_LEFT, 250);
    stepList->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 100);
    main->Add(stepList, 1, wxEXPAND | wxALL, 5);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* runBtn =
        new wxButton(parent, wxID_ANY, isAutoMode ? _("Run All") : _("Run (Step-by-Step)"));
    auto* stopBtn = new wxButton(parent, wxID_ANY, _("Stop"));
    auto* addBtn = new wxButton(parent, wxID_ANY, _("Add"));
    auto* editBtn = new wxButton(parent, wxID_ANY, _("Edit"));
    auto* delBtn = new wxButton(parent, wxID_ANY, _("Delete"));

    auto refreshList = [this, isAutoMode, stepList]() {
        stepList->DeleteAllItems();
        if (!m_sessionConfig) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        int idx = 0;
        for (const auto& s : flow) {
            long item = stepList->InsertItem(idx++, wxString::FromUTF8(s.value("name", "Unnamed")));
            stepList->SetItem(item, 1, _("Pending"));
        }
    };
    refreshList();

    addBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;

        wxDialog dlg(this, wxID_ANY, _("Add Step"));
        auto* sizer = new wxBoxSizer(wxVERTICAL);

        sizer->Add(new wxStaticText(&dlg, wxID_ANY, _("Step Name:")), 0, wxALL, 5);
        auto* nameCtrl = new wxTextCtrl(&dlg, wxID_ANY, "New Step");
        sizer->Add(nameCtrl, 0, wxEXPAND | wxALL, 5);

        sizer->Add(new wxStaticText(&dlg, wxID_ANY, _("Action:")), 0, wxALL, 5);
        wxArrayString actions;
        actions.Add("Read");
        actions.Add("Write");
        actions.Add("Send");
        actions.Add("Delay");
        auto* actionCtrl = new wxChoice(&dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, actions);
        actionCtrl->SetSelection(0);
        sizer->Add(actionCtrl, 0, wxEXPAND | wxALL, 5);

        sizer->Add(new wxStaticText(&dlg, wxID_ANY, _("Variable:")), 0, wxALL, 5);
        wxArrayString varNames;
        for (const auto& [n, v] : m_sessionConfig->variables) varNames.Add(wxString::FromUTF8(n));
        auto* varCtrl = new wxChoice(&dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, varNames);
        if (varNames.GetCount() > 0) varCtrl->SetSelection(0);
        sizer->Add(varCtrl, 0, wxEXPAND | wxALL, 5);

        sizer->Add(new wxStaticText(&dlg, wxID_ANY, _("Assert:")), 0, wxALL, 5);
        auto* assertCtrl = new wxTextCtrl(&dlg, wxID_ANY, "");
        sizer->Add(assertCtrl, 0, wxEXPAND | wxALL, 5);

        if (auto* btns = dlg.CreateButtonSizer(wxOK | wxCANCEL))
            sizer->Add(btns, 0, wxEXPAND | wxALL, 10);
        dlg.SetSizerAndFit(sizer);

        if (dlg.ShowModal() == wxID_OK) {
            nlohmann::json step;
            step["name"] = std::string(nameCtrl->GetValue().utf8_str());
            step["action"] = std::string(actionCtrl->GetStringSelection().utf8_str());
            if (varCtrl->GetSelection() != wxNOT_FOUND)
                step["variable"] = std::string(varCtrl->GetStringSelection().utf8_str());
            step["wait_ms"] = 500;
            if (!assertCtrl->GetValue().empty())
                step["assert"] = {{"type", "eq"},
                                  {"expected", std::string(assertCtrl->GetValue().utf8_str())}};

            flow.push_back(step);
            refreshList();
        }
    });

    editBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;

        JsonEditDialog dlg(this, _("Edit Step (JSON)"), flow[sel]);
        if (dlg.ShowModal() == wxID_OK) {
            flow[sel] = dlg.GetJson();
            refreshList();
        }
    });

    delBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        flow.erase(flow.begin() + sel);
        refreshList();
    });

    runBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, runBtn, stopBtn](wxCommandEvent&) {
        if (!m_sessionConfig || m_testRunner) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        if (!flow.is_array() || flow.empty()) return;

        std::vector<ITestStepPtr> steps;
        for (const auto& stepJson : flow) {
            std::string type = stepJson.value("action", "");
            auto step = m_protocol->CreateTestStep(type, stepJson);
            if (step) steps.push_back(std::move(step));
        }
        if (steps.empty()) {
            wxMessageBox(_("No valid steps."), _("Error"), wxOK);
            return;
        }

        for (int i = 0; i < stepList->GetItemCount(); ++i) {
            stepList->SetItem(i, 1, _("Pending"));
            stepList->SetItemTextColour(i, *wxBLACK);
        }

        m_testRunner =
            new TestRunner(m_protocol, m_session, std::move(steps), m_sessionConfig->variables);
        m_testRunner->SetRunMode(isAutoMode ? TestRunMode::Auto : TestRunMode::SingleStep);

        m_testRunner->OnStepUpdated(
            [this, stepList](int index, bool success, const std::string& msg) {
                wxTheApp->CallAfter([this, stepList, index, success, msg]() {
                    if (index < 0 || index >= stepList->GetItemCount()) return;
                    stepList->SetItem(index, 1, success ? _("Passed") : _("Failed"));
                    stepList->SetItemTextColour(index, success ? *wxGREEN : *wxRED);
                });
            });
        m_testRunner->OnFinished([this, runBtn, stopBtn](bool overallSuccess) {
            wxTheApp->CallAfter([this, runBtn, stopBtn, overallSuccess]() {
                runBtn->Enable(true);
                stopBtn->Enable(false);
                wxMessageBox(overallSuccess ? _("Test Finished Successfully") : _("Test Failed"),
                             _("Result"),
                             wxOK | (overallSuccess ? wxICON_INFORMATION : wxICON_ERROR), this);
                delete m_testRunner;
                m_testRunner = nullptr;
            });
        });
        m_testRunner->OnWaitForNextStep([this, stepList](int nextIndex) {
            wxTheApp->CallAfter([this, stepList, nextIndex]() {
                // 1. 在列表中高亮即将执行的下一步
                if (nextIndex < stepList->GetItemCount()) {
                    stepList->SetItemState(nextIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                    stepList->EnsureVisible(nextIndex);
                }

                // 2. 弹出确认框
                int ret = wxMessageBox(
                    wxString::Format(_("Step %d finished.\n\nReady to execute Step %d. Continue?"),
                                     nextIndex - 1, nextIndex),
                    _("Interactive Mode"), wxYES_NO | wxICON_QUESTION, this);

                if (m_testRunner) {
                    if (ret == wxYES) {
                        m_testRunner->Continue();
                    } else {
                        m_testRunner->Stop();
                    }
                }
            });
        });

        runBtn->Enable(false);
        stopBtn->Enable(true);
        m_testRunner->Start();
    });

    stopBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_testRunner) m_testRunner->Stop();
    });

    btnSizer->Add(runBtn, 0, wxALL, 4);
    btnSizer->Add(stopBtn, 0, wxALL, 4);
    btnSizer->AddSpacer(20);
    btnSizer->Add(addBtn, 0, wxALL, 4);
    btnSizer->Add(editBtn, 0, wxALL, 4);
    btnSizer->Add(delBtn, 0, wxALL, 4);
    main->Add(btnSizer, 0, wxALIGN_LEFT);
    parent->SetSizer(main);
}

void SessionPanel::OnSendTextChanged(wxCommandEvent& evt) {
    if (m_sessionConfig && m_sendTextCtrl) {
        m_sessionConfig->manualMode.sendPayload =
            std::string(m_sendTextCtrl->GetValue().utf8_str());
        if (m_appendNewline)
            m_sessionConfig->manualMode.appendNewline = m_appendNewline->GetValue();
    }
    evt.Skip();
}

void SessionPanel::RefreshFromSession() {
    if (!m_session) return;
    m_titleLabel->SetLabel(
        wxGetTranslation(wxString::FromUTF8(m_session->TransportName().c_str())));
    m_targetLabel->SetLabel(
        wxString::Format(_("Target: %s"), wxString::FromUTF8(m_session->Target().c_str())));
    const SessionState newState = m_session->State();
    if (newState != m_lastState) {
        m_lastState = newState;
        UpdateStateLabel(newState);
        UpdateActionButton(newState);
    }
    Layout();
}

void SessionPanel::UpdateStateLabel(SessionState st) {
    bool isServer = m_streamSession && m_streamSession->IsServerMode();
    switch (st) {
        case SessionState::Idle:
            m_statusLabel->SetLabel(_("Idle"));
            m_statusLabel->SetForegroundColour(wxColour(128, 128, 128));
            break;
        case SessionState::Connecting:
            m_statusLabel->SetLabel(_("Connecting..."));
            m_statusLabel->SetForegroundColour(wxColour(200, 128, 0));
            break;
        case SessionState::Reconnecting:
            m_statusLabel->SetLabel(_("Reconnecting..."));
            m_statusLabel->SetForegroundColour(wxColour(200, 128, 0));
            break;
        case SessionState::Connected:
            if (m_streamSession && isServer)
                m_statusLabel->SetLabel(m_clientCount > 0
                                            ? wxString::Format(_("Connected (%d)"), m_clientCount)
                                            : _("Listening"));
            else
                m_statusLabel->SetLabel(_("Connected"));
            m_statusLabel->SetForegroundColour(wxColour(0, 128, 0));
            break;
        case SessionState::Disconnected:
            m_statusLabel->SetLabel(_("Disconnected"));
            m_statusLabel->SetForegroundColour(wxColour(192, 0, 0));
            break;
    }
}

void SessionPanel::UpdateActionButton(SessionState st) {
    bool isServer = m_streamSession && m_streamSession->IsServerMode();
    switch (st) {
        case SessionState::Idle:
        case SessionState::Disconnected:
            m_actionBtn->SetLabel(_("Open Connection"));
            break;
        case SessionState::Connecting:
            m_actionBtn->SetLabel(_("Cancel"));
            break;
        case SessionState::Reconnecting:
            m_actionBtn->SetLabel(_("Cancel Reconnecting"));
            break;
        case SessionState::Connected:
            m_actionBtn->SetLabel(isServer ? _("Stop Listening") : _("Disconnect"));
            break;
    }
    m_configBtn->Enable(st == SessionState::Idle || st == SessionState::Disconnected);
    if (m_streamSession) {
        bool canSend = (st == SessionState::Connected);
        m_sendBtn->Enable(canSend);
        if (m_sendTextCtrl) m_sendTextCtrl->Enable(canSend);
    }
    if (m_txSession) {
        if (m_executeReadBtn) m_executeReadBtn->Enable(st == SessionState::Connected);
        if (m_executeWriteBtn) m_executeWriteBtn->Enable(st == SessionState::Connected);
    }
}

void SessionPanel::OnAction(wxCommandEvent&) {
    if (!m_session) return;
    switch (m_session->State()) {
        case SessionState::Idle:
        case SessionState::Disconnected:
            if (m_onRequestOpen)
                m_onRequestOpen();
            else {
                m_session->Open();
                RefreshFromSession();
            }
            break;
        case SessionState::Connecting:
        case SessionState::Reconnecting:
        case SessionState::Connected:
            m_session->Close();
            RefreshFromSession();
            break;
    }
}

void SessionPanel::OnConfigure(wxCommandEvent&) {
    if (m_onRequestConfigure) m_onRequestConfigure();
}

void SessionPanel::BindStreamCallbacks() {
    m_streamSession->OnMessage([this](int cid, const std::vector<uint8_t>& msg) {
        auto copy = msg;
        CallAfter([this, cid, copy = std::move(copy)]() {
            m_rxBytes += copy.size();
            m_rxLabel->SetLabel(
                wxString::Format(_("Received: %llu bytes"), (unsigned long long)m_rxBytes));
            AppendLine(cid > 0 ? wxString::Format("<#%d", cid) : "<", FormatMessage(copy));
        });
    });
    m_streamSession->OnReconnectAttempt([this](int attempt) {
        CallAfter([this, attempt]() {
            AppendLine("", wxString::Format(_("Reconnecting (attempt %d) ..."), attempt));
        });
    });
    m_streamSession->OnClientConnected([this](int cid) {
        CallAfter([this, cid]() {
            ++m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            AppendLine("", wxString::Format(_("Client #%d connected"), cid));
        });
    });
    m_streamSession->OnClientDisconnected([this](int cid) {
        CallAfter([this, cid]() {
            if (m_clientCount > 0) --m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            AppendLine("", wxString::Format(_("Client #%d disconnected"), cid));
        });
    });
    m_streamSession->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            RefreshFromSession();
            if (m_onStateUpdated) m_onStateUpdated(st);
        });
    });
}

wxString SessionPanel::FormatMessage(const std::vector<uint8_t>& msg) {
    if (m_hexDisplay && m_hexDisplay->IsChecked()) {
        wxString text;
        for (uint8_t b : msg) text += wxString::Format("%02X ", b);
        if (!text.IsEmpty()) text.RemoveLast();
        return text;
    }
    return wxString::FromUTF8(reinterpret_cast<const char*>(msg.data()), msg.size());
}

void SessionPanel::AppendLine(const wxString& prefix, const wxString& content) {
    if (!m_dataLogTextCtrl) return;
    wxString line;
    if (m_showTimestamp && m_showTimestamp->IsChecked())
        line = wxDateTime::Now().Format("[%Y-%m-%d %H:%M:%S] ");
    if (!prefix.IsEmpty()) line += prefix + " ";
    line += content + "\n";
    m_dataLogTextCtrl->AppendText(line);
    if (!m_autoScroll || m_autoScroll->IsChecked()) m_dataLogTextCtrl->SetInsertionPointEnd();
}

void SessionPanel::OnSend(wxCommandEvent&) {
    if (!m_streamSession || !m_streamSession->IsConnected()) return;
    wxString text = m_sendTextCtrl->GetValue();
    if (text.IsEmpty()) return;
    std::string utf8 = std::string(text.utf8_str());
    if (m_appendNewline && m_appendNewline->IsChecked()) utf8 += "\n";
    std::vector<uint8_t> bytes(utf8.begin(), utf8.end());
    if (m_streamSession->Send(bytes)) {
        m_txBytes += bytes.size();
        m_txLabel->SetLabel(wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));
        AppendLine(">", text);
        m_sendTextCtrl->Clear();
    } else {
        AppendLine("!", _("Send failed"));
    }
}

void SessionPanel::OnSendEnter(wxCommandEvent& evt) {
    OnSend(evt);
}
void SessionPanel::OnClear(wxCommandEvent&) {
    if (m_dataLogTextCtrl) m_dataLogTextCtrl->Clear();
}
void SessionPanel::UpdateClientsLabel() {
    if (m_clientsLabel) m_clientsLabel->SetLabel(wxString::Format(_("Clients: %d"), m_clientCount));
}

void SessionPanel::BindTransactionCallbacks() {
    m_txSession->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            RefreshFromSession();
            if (m_onStateUpdated) m_onStateUpdated(st);
        });
    });
    m_txSession->OnLog([this](const std::string& msg) {
        CallAfter([this, msg]() { AppendTxLog(wxString::FromUTF8(msg.c_str())); });
    });
}

void SessionPanel::AppendTxLog(const wxString& line) {
    if (m_txLogTextCtrl) {
        m_txLogTextCtrl->AppendText(line + "\n");
        m_txLogTextCtrl->SetInsertionPointEnd();
    }
}

}  // namespace icu
