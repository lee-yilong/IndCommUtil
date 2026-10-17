#include "SessionPanel.h"

#include <wx/datetime.h>
#include <wx/tokenzr.h>

#include <thread>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

SessionPanel::SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session)
    : wxPanel(parent, wxID_ANY), m_protocol(protocol), m_modeIndex(modeIndex), m_session(session) {
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
}

SessionPanel::~SessionPanel() {
    // 完全为空，防止野指针访问！
}

void SessionPanel::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);
    BuildCommonUi(main);

    if (m_streamSession) {
        BuildStreamUi(main);
    } else if (m_txSession) {
        BuildTransactionUi(main);
    }

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

void SessionPanel::BuildStreamUi(wxBoxSizer* mainSizer) {
    if (m_streamSession->IsServerMode()) {
        m_clientsLabel->Show();
    } else {
        m_clientsLabel->Hide();
    }

    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Data Log"));
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

    auto* sendBox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Send"));
    m_sendTextCtrl = new wxTextCtrl(sendBox->GetStaticBox(), wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_sendTextCtrl->Bind(wxEVT_TEXT_ENTER, &SessionPanel::OnSendEnter, this);
    m_sendBtn = new wxButton(sendBox->GetStaticBox(), wxID_ANY, _("Send"));
    m_sendBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnSend, this);
    m_appendNewline = new wxCheckBox(sendBox->GetStaticBox(), wxID_ANY, _("Append newline"));
    m_appendNewline->SetValue(true);

    sendBox->Add(m_sendTextCtrl, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_appendNewline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_sendBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    mainSizer->Add(sendBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
}

void SessionPanel::BuildTransactionUi(wxBoxSizer* mainSizer) {
    m_rxLabel->Hide();
    m_txLabel->Hide();
    m_clientsLabel->Hide();

    auto fields = m_protocol->OperationFieldsForMode(m_modeIndex);

    auto* reqBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Operation Request"));
    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    for (const auto& fd : fields) {
        grid->Add(new wxStaticText(reqBox->GetStaticBox(), wxID_ANY,
                                   wxGetTranslation(wxString::FromUTF8(fd.label))),
                  0, wxALIGN_CENTER_VERTICAL);
        wxWindow* ctrl = CreateControl(reqBox->GetStaticBox(), fd);
        grid->Add(ctrl, 1, wxEXPAND);
        m_operationControls[fd.key] = ctrl;
    }
    reqBox->Add(grid, 1, wxEXPAND | wxALL, 8);

    m_executeBtn = new wxButton(reqBox->GetStaticBox(), wxID_ANY, _("Execute"));
    m_executeBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnExecute, this);
    reqBox->Add(m_executeBtn, 0, wxALL, 8);
    mainSizer->Add(reqBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    // --- 新增：绑定功能码下拉框的事件 ---
    auto it = m_operationControls.find("functionCode");
    if (it != m_operationControls.end()) {
        auto* ch = dynamic_cast<wxChoice*>(it->second);
        if (ch) {
            ch->Bind(wxEVT_CHOICE, &SessionPanel::OnFunctionCodeChanged, this);
            // 初始化时调用一次，设置正确的状态
            CallAfter([this]() { UpdateOperationUiState(); });
        }
    }
    // ---------------------------------

    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Result Log"));
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
        case FieldValue::Type::String: {
            return new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(fd.defaultValue.AsString()));
        }
        case FieldValue::Type::Int: {
            return new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, fd.minValue, fd.maxValue,
                                  fd.defaultValue.AsInt());
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
            if (m_streamSession && isServer) {
                m_statusLabel->SetLabel(m_clientCount > 0
                                            ? wxString::Format(_("Connected (%d)"), m_clientCount)
                                            : _("Listening"));
            } else {
                m_statusLabel->SetLabel(_("Connected"));
            }
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
    const bool canConfig = (st == SessionState::Idle || st == SessionState::Disconnected);
    m_configBtn->Enable(canConfig);

    if (m_streamSession) {
        bool canSend = (st == SessionState::Connected);
        m_sendBtn->Enable(canSend);
        m_sendTextCtrl->Enable(canSend);
    }
    if (m_txSession) {
        m_executeBtn->Enable(st == SessionState::Connected);
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
            if (m_onStateUpdated) m_onStateUpdated(st);  // 通知 MainFrame
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

    // 绑定底层发来的日志（比如心跳断线、正在重连）
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

void SessionPanel::OnExecute(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;

    std::map<std::string, FieldValue> fields;
    auto descriptors = m_protocol->OperationFieldsForMode(m_modeIndex);
    for (const auto& fd : descriptors) {
        auto it = m_operationControls.find(fd.key);
        if (it == m_operationControls.end()) continue;
        wxWindow* w = it->second;
        switch (fd.type) {
            case FieldValue::Type::String:
                fields[fd.key] = std::string(static_cast<wxTextCtrl*>(w)->GetValue().utf8_str());
                break;
            case FieldValue::Type::Int:
                fields[fd.key] = static_cast<wxSpinCtrl*>(w)->GetValue();
                break;
            case FieldValue::Type::Bool:
                fields[fd.key] = static_cast<wxCheckBox*>(w)->GetValue();
                break;
            case FieldValue::Type::Choice: {
                auto* ch = static_cast<wxChoice*>(w);
                int idx = ch->GetSelection();
                if (idx >= 0 && idx < (int)fd.choices.size())
                    fields[fd.key] = fd.choices[idx].first;
                break;
            }
        }
    }

    AppendTxLog(_("Executing request..."));
    // 关键：把请求丢到后台线程执行
    std::thread([this, fields]() {
        TransactionResponse resp = m_txSession->Execute(fields);

        // 执行完后，用 CallAfter 回到主线程更新 UI
        CallAfter([this, resp]() {
            if (resp.ok) {
                wxString resStr = "Success: ";
                if (!resp.registers.empty()) {
                    for (auto v : resp.registers) resStr += wxString::Format("%04X ", v);
                } else if (!resp.coils.empty()) {
                    for (auto b : resp.coils) resStr += b ? "1 " : "0 ";
                }
                AppendTxLog(resStr);
            } else {
                AppendTxLog(
                    wxString::Format("Error: %s", wxString::FromUTF8(resp.errorMessage.c_str())));
            }
        });
    }).detach();  // detach 让线程在后台独立运行，不用主线程 join
}

void SessionPanel::OnFunctionCodeChanged(wxCommandEvent&) {
    UpdateOperationUiState();
}

void SessionPanel::UpdateOperationUiState() {
    auto it = m_operationControls.find("functionCode");
    if (it == m_operationControls.end()) return;

    auto* ch = dynamic_cast<wxChoice*>(it->second);
    if (!ch) return;

    int sel = ch->GetSelection();
    if (sel == wxNOT_FOUND) return;

    // 根据选中的项获取功能码字符串（"1", "3", "15" 等）
    auto fields = m_protocol->OperationFieldsForMode(m_modeIndex);
    std::string funcCodeStr;
    for (const auto& fd : fields) {
        if (fd.key == "functionCode") {
            if (sel >= 0 && sel < (int)fd.choices.size()) {
                funcCodeStr = fd.choices[sel].first;
            }
            break;
        }
    }
    if (funcCodeStr.empty()) return;
    int fc = std::stoi(funcCodeStr);

    // 1. 读操作 (01, 02, 03, 04)：需要 Count，不需要 Write Values
    // 2. 写单操作 (05, 06)：不需要 Count，只需要 Write Values 的第一个
    // 3. 写多操作 (15, 16)：不需要 Count，需要 Write Values
    bool isRead = (fc == 1 || fc == 2 || fc == 3 || fc == 4);
    bool isWriteSingle = (fc == 5 || fc == 6);
    bool isWriteMultiple = (fc == 15 || fc == 16);

    auto itCount = m_operationControls.find("count");
    if (itCount != m_operationControls.end()) {
        itCount->second->Enable(isRead);  // 只有读操作才需要填 Count
    }

    auto itWrite = m_operationControls.find("writeValues");
    if (itWrite != m_operationControls.end()) {
        itWrite->second->Enable(!isRead);  // 读操作不需要填写入值
    }
}

}  // namespace icu
