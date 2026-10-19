#include "SessionPanel.h"

#include <wx/datetime.h>
#include <wx/tokenzr.h>

#include <thread>

#include "TestStepEditDialog.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

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

    // 1. 先创建 Splitter（父窗口是 this）
    m_mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);

    // =========================================================================
    // 左侧：全局通讯变量池（父窗口改成 m_mainSplitter）
    // =========================================================================
    auto* varPanel = new wxPanel(m_mainSplitter);
    auto* varSizer = new wxBoxSizer(wxVERTICAL);

    auto* varBox = new wxStaticBoxSizer(wxVERTICAL, varPanel, _("Communication Variables"));
    m_varList = new wxListCtrl(varBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL);
    m_varList->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 100);
    m_varList->InsertColumn(1, _("Type"), wxLIST_FORMAT_LEFT, 60);
    m_varList->InsertColumn(2, _("Access"), wxLIST_FORMAT_LEFT, 60);
    m_varList->InsertColumn(3, _("Value"), wxLIST_FORMAT_LEFT, 80);
    m_varList->Bind(wxEVT_LIST_ITEM_SELECTED, &SessionPanel::OnVarSelected, this);
    varBox->Add(m_varList, 1, wxEXPAND | wxALL, 4);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_addVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Add"));
    m_editVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Edit"));
    m_delVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Delete"));
    m_addVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnAddVariable, this);
    m_editVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnEditVariable, this);
    m_delVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnDeleteVariable, this);

    btnSizer->Add(m_addVarBtn, 0, wxALL, 2);
    btnSizer->Add(m_editVarBtn, 0, wxALL, 2);
    btnSizer->Add(m_delVarBtn, 0, wxALL, 2);
    varBox->Add(btnSizer, 0, wxALIGN_RIGHT);

    varSizer->Add(varBox, 1, wxEXPAND | wxALL, 5);
    varPanel->SetSizer(varSizer);
    RefreshVariableList();

    // =========================================================================
    // 右侧：模式选项卡（父窗口改成 m_mainSplitter）
    // =========================================================================
    m_modeNotebook = new wxNotebook(m_mainSplitter, wxID_ANY);
    m_manualPage = new wxPanel(m_modeNotebook);
    m_interactivePage = new wxPanel(m_modeNotebook);
    m_autoPage = new wxPanel(m_modeNotebook);

    m_modeNotebook->AddPage(m_manualPage, _("Manual Mode"));
    m_modeNotebook->AddPage(m_interactivePage, _("Interactive Mode"));
    m_modeNotebook->AddPage(m_autoPage, _("Auto Mode"));
    m_modeNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SessionPanel::OnNotebookPageChanged, this);

    // --- 手动模式页面布局 ---
    auto* manualSizer = new wxBoxSizer(wxVERTICAL);

    // 操作区
    m_varOpSizer = new wxStaticBoxSizer(wxVERTICAL, m_manualPage, _("Operation Request"));
    auto* hint = new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                  _("Please select a variable from the left list."));
    m_varOpSizer->Add(hint, 0, wxALL, 10);
    manualSizer->Add(m_varOpSizer, 0, wxEXPAND | wxALL, 5);  // 0 表示不拉伸，高度随内容

    // 日志区
    if (m_streamSession)
        BuildStreamUi(manualSizer, m_manualPage);
    else if (m_txSession)
        BuildTransactionUi(manualSizer, m_manualPage);

    m_manualPage->SetSizer(manualSizer);

    BuildTestFlowUi(m_interactivePage, false);
    BuildTestFlowUi(m_autoPage, true);

    // =========================================================================
    // 3. 最后执行分栏（此时它们都已经是 m_mainSplitter 的孩子了）
    // =========================================================================
    m_mainSplitter->SplitVertically(varPanel, m_modeNotebook, 320);
    m_mainSplitter->SetMinimumPaneSize(200);

    main->Add(m_mainSplitter, 1, wxEXPAND | wxALL, 5);
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

    // 日志区
    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Data Log"));

    // 工具栏
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

    // 把工具栏加到 logBox
    logBox->Add(logToolbar, 0, wxEXPAND);

    // 文本框
    m_dataLogTextCtrl =
        new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                       wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    logBox->Add(m_dataLogTextCtrl, 1, wxEXPAND | wxALL, 4);

    // 把 logBox 加到主 Sizer（1 表示占据剩余所有垂直空间）
    mainSizer->Add(logBox, 1, wxEXPAND | wxALL, 5);
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

wxWindow* SessionPanel::CreateJsonControl(wxWindow* parent, const FieldDescriptor& fd,
                                          const nlohmann::json& currentValue,
                                          std::function<void(const nlohmann::json&)> onChanged) {
    switch (fd.type) {
        case FieldValue::Type::String: {
            wxString defStr = currentValue.is_string()
                                  ? wxString::FromUTF8(currentValue.get<std::string>())
                                  : wxString::FromUTF8(fd.defaultValue.AsString());
            auto* t = new wxTextCtrl(parent, wxID_ANY, defStr);
            t->Bind(wxEVT_TEXT, [t, onChanged](wxCommandEvent&) {
                onChanged(std::string(t->GetValue().utf8_str()));
            });
            return t;
        }
        case FieldValue::Type::Int: {
            int defInt =
                currentValue.is_number() ? currentValue.get<int>() : fd.defaultValue.AsInt();
            auto* s = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, fd.minValue, fd.maxValue, defInt);
            s->Bind(wxEVT_SPINCTRL, [s, onChanged](wxSpinEvent&) { onChanged(s->GetValue()); });
            // 补充：如果用户直接键盘输入不按箭头，在失焦时也能同步
            s->Bind(wxEVT_KILL_FOCUS, [s, onChanged](wxFocusEvent& e) {
                onChanged(s->GetValue());
                e.Skip();
            });
            return s;
        }
        case FieldValue::Type::Bool: {
            bool defBool =
                currentValue.is_boolean() ? currentValue.get<bool>() : fd.defaultValue.AsBool();
            auto* c = new wxCheckBox(parent, wxID_ANY, "");
            c->SetValue(defBool);
            c->Bind(wxEVT_CHECKBOX, [c, onChanged](wxCommandEvent&) { onChanged(c->GetValue()); });
            return c;
        }
        case FieldValue::Type::Choice: {
            wxArrayString labels;
            int sel = 0;
            std::string curChoice = currentValue.is_string() ? currentValue.get<std::string>()
                                                             : fd.defaultValue.AsString();
            for (size_t i = 0; i < fd.choices.size(); ++i) {
                labels.Add(wxGetTranslation(wxString::FromUTF8(fd.choices[i].second)));
                if (fd.choices[i].first == curChoice) sel = (int)i;
            }
            auto* ch = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, labels);
            ch->SetSelection(sel);
            ch->Bind(wxEVT_CHOICE, [ch, fd, onChanged](wxCommandEvent&) {
                int idx = ch->GetSelection();
                if (idx >= 0 && idx < (int)fd.choices.size()) {
                    onChanged(fd.choices[idx].first);
                }
            });
            return ch;
        }
    }
    return new wxTextCtrl(parent, wxID_ANY);
}

void SessionPanel::BuildVarOpUi(const CommVariable& var) {
    m_varOpSizer->Clear(true);
    m_varOpControls.clear();

    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    // 1. 渲染协议专属参数
    auto fields = m_protocol->VariableParamsFields();
    for (const auto& fd : fields) {
        grid->Add(new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                   wxGetTranslation(wxString::FromUTF8(fd.label))),
                  0, wxALIGN_CENTER_VERTICAL);

        nlohmann::json curVal = var.params.contains(fd.key) ? var.params[fd.key] : nlohmann::json();
        wxWindow* ctrl = CreateJsonControl(
            m_varOpSizer->GetStaticBox(), fd, curVal,
            [this, key = fd.key](const nlohmann::json& newVal) {
                if (!m_selectedVarName.empty() && m_sessionConfig) {
                    m_sessionConfig->variables[m_selectedVarName].params[key] = newVal;
                }
            });
        grid->Add(ctrl, 1, wxEXPAND);
        m_varOpControls[fd.key] = ctrl;
    }

    // 2. 统一的值显示/输入框
    grid->Add(new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Value:")), 0,
              wxALIGN_CENTER_VERTICAL);

    auto* valueCtrl = new wxTextCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                     wxString::FromUTF8(var.currentValue));
    valueCtrl->Bind(wxEVT_TEXT, [this, valueCtrl](wxCommandEvent&) {
        if (!m_selectedVarName.empty() && m_sessionConfig) {
            m_sessionConfig->variables[m_selectedVarName].currentValue =
                std::string(valueCtrl->GetValue().utf8_str());
        }
    });

    if (var.access == "RO") {
        valueCtrl->SetEditable(false);
        valueCtrl->SetBackgroundColour(wxColour(240, 240, 240));
    }

    grid->Add(valueCtrl, 1, wxEXPAND);
    m_varOpControls["__value"] = valueCtrl;

    m_varOpSizer->Add(grid, 1, wxEXPAND | wxALL, 8);

    // 3. 按钮区
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    if (m_streamSession) {
        if (var.access != "RO") {
            auto* sendBtn = new wxButton(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Send"));
            sendBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecuteSend, this);
            btnSizer->Add(sendBtn, 0, wxALL, 4);
        }
    } else if (m_txSession) {
        m_executeReadBtn = new wxButton(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Read"));
        m_executeWriteBtn = new wxButton(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Write"));
        m_executeReadBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecuteRead, this);
        m_executeWriteBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecuteWrite, this);

        if (var.access == "RO") m_executeWriteBtn->Enable(false);
        if (var.access == "WO") m_executeReadBtn->Enable(false);

        btnSizer->Add(m_executeReadBtn, 0, wxALL, 4);
        btnSizer->Add(m_executeWriteBtn, 0, wxALL, 4);
    }

    m_varOpSizer->Add(btnSizer, 0, wxALIGN_RIGHT);

    // 强制重新布局页面
    m_manualPage->Layout();
}

void SessionPanel::OnExecuteRead(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;

    // 直接使用已经同步好的变量参数
    auto varCopy = m_sessionConfig->variables[m_selectedVarName];

    TransactionRequest req;
    req.slaveId = varCopy.params.value("slaveId", 1);

    // 如果 params 里存的是字符串形式的功能码，这里兼容解析一下
    if (varCopy.params.contains("functionCode")) {
        if (varCopy.params["functionCode"].is_string()) {
            req.functionCode = std::stoi(varCopy.params["functionCode"].get<std::string>());
        } else {
            req.functionCode = varCopy.params["functionCode"].get<int>();
        }
    }

    req.startAddress = varCopy.params.value("startAddress", 0);
    req.count = varCopy.params.value("count", 1);

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
                    m_sessionConfig->variables[name].currentValue = valStr;
                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                    txt->ChangeValue(valStr);
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

    auto varCopy = m_sessionConfig->variables[m_selectedVarName];

    std::string targetVal =
        std::string(dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])->GetValue().utf8_str());

    TransactionRequest req;
    req.slaveId = varCopy.params.value("slaveId", 1);

    int fc = 6;
    if (varCopy.params.contains("functionCode")) {
        if (varCopy.params["functionCode"].is_string()) {
            fc = std::stoi(varCopy.params["functionCode"].get<std::string>());
        } else {
            fc = varCopy.params["functionCode"].get<int>();
        }
    }

    // 智能映射（保留你原有的写功能码映射逻辑）
    if (fc == 1)
        fc = 5;
    else if (fc == 3)
        fc = 6;
    else if (fc == 4)
        fc = 6;

    req.functionCode = fc;
    req.startAddress = varCopy.params.value("startAddress", 0);
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

    // ★ 不再硬编码 Modbus 默认参数
    nlohmann::json defaultJson = {
        {"type", "int"}, {"access", "RW"}, {"params", nlohmann::json::object()}};

    // ★ 传入 m_protocol
    VariableEditDialog dlg(this, m_protocol, "new_var", defaultJson, false);
    if (dlg.ShowModal() == wxID_OK) {
        std::string name = dlg.GetVarName();
        if (name.empty()) {
            wxMessageBox(_("Variable name cannot be empty."), _("Error"), wxOK | wxICON_ERROR);
            return;
        }
        auto j = dlg.GetVarJson();
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

    // ★ 传入 m_protocol
    VariableEditDialog dlg(this, m_protocol, key, editJson, true);

    if (dlg.ShowModal() == wxID_OK) {
        auto j = dlg.GetVarJson();
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

    // ★ 新增：上移和下移按钮
    auto* upBtn = new wxButton(parent, wxID_ANY, _("Up"));
    auto* downBtn = new wxButton(parent, wxID_ANY, _("Down"));

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
        std::vector<std::string> varNames;
        for (const auto& [n, v] : m_sessionConfig->variables) varNames.push_back(n);

        TestStepEditDialog dlg(this, nlohmann::json::object(), varNames);
        if (dlg.ShowModal() == wxID_OK) {
            nlohmann::json step = dlg.GetStepJson();
            auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                    : m_sessionConfig->interactiveMode.testFlow;

            // ★ 修改：判断是否选中了某一行，有则插入其后，无则追加末尾
            long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            if (sel != -1 && sel < static_cast<long>(flow.size())) {
                flow.insert(flow.begin() + sel + 1, step);
            } else {
                flow.push_back(step);
            }

            refreshList();

            // 插入后自动选中新插入的行
            int newIdx = (sel != -1) ? sel + 1 : stepList->GetItemCount() - 1;
            stepList->SetItemState(newIdx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            stepList->EnsureVisible(newIdx);
        }
    });

    editBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) return;

        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;

        std::vector<std::string> varNames;
        for (const auto& [n, v] : m_sessionConfig->variables) varNames.push_back(n);

        TestStepEditDialog dlg(this, flow[sel], varNames);
        if (dlg.ShowModal() == wxID_OK) {
            flow[sel] = dlg.GetStepJson();
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

    // ★ 新增：上移逻辑
    upBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel <= 0) return;  // 未选中或已经是第一行

        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;

        std::swap(flow[sel], flow[sel - 1]);
        refreshList();

        // 保持选中状态跟随
        stepList->SetItemState(sel - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        stepList->EnsureVisible(sel - 1);
    });

    // ★ 新增：下移逻辑
    downBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        if (sel == -1 || sel >= static_cast<long>(flow.size()) - 1) return;  // 未选中或已是最后一行

        std::swap(flow[sel], flow[sel + 1]);
        refreshList();

        stepList->SetItemState(sel + 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        stepList->EnsureVisible(sel + 1);
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
        m_testRunner->OnStepLog([this](const std::string& msg) {
            CallAfter([this, msg]() {
                if (m_dataLogTextCtrl) {
                    // TCP/串口模式下，打到数据日志
                    AppendLine("", wxString::FromUTF8(msg));
                } else if (m_txLogTextCtrl) {
                    // Modbus 模式下，打到结果日志
                    AppendTxLog(wxString::FromUTF8(msg));
                }
            });
        });
        m_testRunner->SetRunMode(isAutoMode ? TestRunMode::Auto : TestRunMode::SingleStep);

        m_testRunner->OnStepUpdated(
            [this, stepList](int index, bool success, const std::string& msg) {
                wxTheApp->CallAfter([this, stepList, index, success, msg]() {
                    if (index < 0 || index >= stepList->GetItemCount()) return;
                    stepList->SetItem(index, 1, success ? _("Passed") : _("Failed"));
                    stepList->SetItemTextColour(index, success ? *wxGREEN : *wxRED);

                    // ★ 新增：每次步骤执行完（变量已被 Extract 更新），刷新左侧变量池
                    RefreshVariableList();
                });
            });
        m_testRunner->OnFinished([this, runBtn, stopBtn](bool overallSuccess) {
            wxTheApp->CallAfter([this, runBtn, stopBtn, overallSuccess]() {
                runBtn->Enable(true);
                stopBtn->Enable(false);
                wxMessageBox(overallSuccess ? _("Test Finished Successfully") : _("Test Failed"),
                             _("Result"),
                             wxOK | (overallSuccess ? wxICON_INFORMATION : wxICON_ERROR), this);
                if (m_testRunner) {
                    delete m_testRunner;
                    m_testRunner = nullptr;
                }
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
    btnSizer->Add(upBtn, 0, wxALL, 4);
    btnSizer->Add(downBtn, 0, wxALL, 4);
    main->Add(btnSizer, 0, wxALIGN_LEFT);
    parent->SetSizer(main);
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
            if (m_rxLabel)
                m_rxLabel->SetLabel(
                    wxString::Format(_("Received: %llu bytes"), (unsigned long long)m_rxBytes));

            // ★ 日志记录
            AppendLine(cid > 0 ? wxString::Format("<#%d", cid) : "<", FormatMessage(copy));

            // ★ 覆盖变量值（只要不是只写 WO）
            if (!m_selectedVarName.empty() && m_sessionConfig &&
                m_sessionConfig->variables.count(m_selectedVarName)) {
                auto& var = m_sessionConfig->variables[m_selectedVarName];
                if (var.access != "WO") {
                    std::string text(copy.begin(), copy.end());
                    var.currentValue = text;  // 覆盖
                    RefreshVariableList();

                    // 刷新操作区输入框的值 (用 ChangeValue 避免触发 wxEVT_TEXT 导致死循环)
                    if (m_varOpControls.count("__value")) {
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])) {
                            txt->ChangeValue(wxString::FromUTF8(text));
                        }
                    }
                }
            }
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

void SessionPanel::OnExecuteSend(wxCommandEvent&) {
    if (!m_streamSession || !m_streamSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;

    auto& var = m_sessionConfig->variables[m_selectedVarName];
    std::string payload = var.currentValue;  // ★ 直接取唯一的值

    if (payload.empty()) return;  // 允许发空包的话可以去掉此行

    std::vector<uint8_t> bytes(payload.begin(), payload.end());
    if (m_streamSession->Send(bytes)) {
        m_txBytes += bytes.size();
        if (m_txLabel)
            m_txLabel->SetLabel(
                wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));
        AppendLine(">", wxString::FromUTF8(payload));  // 日志记录
    } else {
        AppendLine("!", _("Send failed"));
    }
}

}  // namespace icu
