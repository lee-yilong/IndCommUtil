#include "ProtocolSessionPanel.h"

#include <wx/datetime.h>

#include <string>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

ProtocolSessionPanel::ProtocolSessionPanel(wxWindow* parent, Session* session)
    : wxPanel(parent, wxID_ANY), m_session(session) {
    BuildUi();

    if (m_session) {
        UpdateModeLabel();
    }

    if (m_session && m_session->IsServerMode()) {
        m_clientsLabel->Show();
        UpdateClientsLabel();
    } else {
        m_clientsLabel->Hide();
    }

    if (m_session) {
        m_lastState = m_session->State();
        UpdateStateLabel(m_lastState);
        UpdateActionButton(m_lastState);

        m_targetLabel->SetLabel(
            wxString::Format(_("Target: %s"), wxString::FromUTF8(m_session->Target().c_str())));

        if (m_lastState == SessionState::Connecting) {
            AppendLine("", wxString::Format(_("Connecting to %s ..."),
                                            wxString::FromUTF8(m_session->Target().c_str())));
        }
    }
}

ProtocolSessionPanel::~ProtocolSessionPanel() = default;

void ProtocolSessionPanel::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);

    m_modeLabel = new wxStaticText(this, wxID_ANY, "-");
    {
        wxFont f = m_modeLabel->GetFont();
        f.SetPointSize(f.GetPointSize() + 3);
        f.MakeBold();
        m_modeLabel->SetFont(f);
    }
    m_modeLabel->SetForegroundColour(wxColour(0, 0, 160));
    main->Add(m_modeLabel, 0, wxLEFT | wxRIGHT | wxTOP, 10);

    auto* header = new wxStaticBoxSizer(wxVERTICAL, this, _("Session Status"));
    m_statusLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, "-");
    wxFont f = m_statusLabel->GetFont();
    f.MakeBold();
    f.SetPointSize(f.GetPointSize() + 2);
    m_statusLabel->SetFont(f);
    header->Add(m_statusLabel, 0, wxALL, 8);

    m_targetLabel = new wxStaticText(header->GetStaticBox(), wxID_ANY, "-");
    header->Add(m_targetLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    main->Add(header, 0, wxEXPAND | wxALL, 10);

    auto* stats = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Statistics"));
    m_rxLabel = new wxStaticText(stats->GetStaticBox(), wxID_ANY, _("Received: 0 bytes"));
    m_txLabel = new wxStaticText(stats->GetStaticBox(), wxID_ANY, _("Sent: 0 bytes"));
    m_clientsLabel = new wxStaticText(stats->GetStaticBox(), wxID_ANY, _("Clients: 0"));

    stats->Add(m_rxLabel, 1, wxALL, 6);
    stats->Add(m_txLabel, 1, wxALL, 6);
    stats->Add(m_clientsLabel, 1, wxALL, 6);
    main->Add(stats, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, this, _("Data Log"));

    // 阶段 5.5：显示工具栏（HEX / 时间戳 / 自动滚动 / 清空）
    auto* logToolbar = new wxBoxSizer(wxHORIZONTAL);
    m_hexDisplay = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("HEX Display"));
    m_showTimestamp = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Timestamp"));
    m_autoScroll = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Auto Scroll"));
    m_clearBtn = new wxButton(logBox->GetStaticBox(), wxID_ANY, _("Clear"));

    m_hexDisplay->SetValue(false);
    m_showTimestamp->SetValue(true);
    m_autoScroll->SetValue(true);

    m_clearBtn->Bind(wxEVT_BUTTON, &ProtocolSessionPanel::OnClear, this);

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
    main->Add(logBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* sendBox = new wxStaticBoxSizer(wxHORIZONTAL, this, _("Send"));
    m_sendTextCtrl = new wxTextCtrl(sendBox->GetStaticBox(), wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_sendTextCtrl->Bind(wxEVT_TEXT_ENTER, &ProtocolSessionPanel::OnSendEnter, this);

    m_sendBtn = new wxButton(sendBox->GetStaticBox(), wxID_ANY, _("Send"));
    m_sendBtn->Bind(wxEVT_BUTTON, &ProtocolSessionPanel::OnSend, this);

    m_appendNewline = new wxCheckBox(sendBox->GetStaticBox(), wxID_ANY, _("Append newline"));
    m_appendNewline->SetValue(true);

    sendBox->Add(m_sendTextCtrl, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_appendNewline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    sendBox->Add(m_sendBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    main->Add(sendBox, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);

    m_actionBtn = new wxButton(this, wxID_ANY, _("Open Connection"));
    m_actionBtn->Bind(wxEVT_BUTTON, &ProtocolSessionPanel::OnAction, this);
    btnSizer->Add(m_actionBtn, 0, wxRIGHT, 8);

    m_configBtn = new wxButton(this, wxID_ANY, _("Configure Connection..."));
    m_configBtn->Bind(wxEVT_BUTTON, &ProtocolSessionPanel::OnConfigure, this);
    btnSizer->Add(m_configBtn, 0);

    main->Add(btnSizer, 0, wxALL, 10);

    SetSizer(main);
}

void ProtocolSessionPanel::RefreshFromSession() {
    if (!m_session) return;

    UpdateModeLabel();

    const SessionState newState = m_session->State();
    if (newState != m_lastState) {
        AppendNotice(m_lastState, newState);
        m_lastState = newState;
    }

    UpdateStateLabel(newState);
    UpdateActionButton(newState);

    m_targetLabel->SetLabel(
        wxString::Format(_("Target: %s"), wxString::FromUTF8(m_session->Target().c_str())));

    Layout();
}

void ProtocolSessionPanel::UpdateStateLabel(SessionState st) {
    const bool isServer = m_session && m_session->IsServerMode();

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
            if (isServer) {
                if (m_clientCount > 0)
                    m_statusLabel->SetLabel(wxString::Format(_("Connected (%d)"), m_clientCount));
                else
                    m_statusLabel->SetLabel(_("Listening"));
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

void ProtocolSessionPanel::AppendNotice(SessionState oldSt, SessionState newSt) {
    wxString notice;
    switch (newSt) {
        case SessionState::Connecting:
            notice = wxString::Format(_("Connecting to %s ..."),
                                      wxString::FromUTF8(m_session->Target().c_str()));
            break;
        case SessionState::Reconnecting:
            if (oldSt != SessionState::Reconnecting)
                notice = _("Connection lost, reconnecting ...");
            else
                return;
            break;
        case SessionState::Connected:
            if (m_session && m_session->IsServerMode())
                notice = wxString::Format(_("Listening on %s"),
                                          wxString::FromUTF8(m_session->Target().c_str()));
            else
                notice = wxString::Format(_("Connected to %s"),
                                          wxString::FromUTF8(m_session->Target().c_str()));
            break;
        case SessionState::Disconnected:
            if (oldSt == SessionState::Connecting)
                notice = _("Connection failed or timed out");
            else if (oldSt == SessionState::Reconnecting)
                notice = _("Reconnect failed, giving up");
            else
                notice = _("Disconnected");
            break;
        default:
            return;
    }

    AppendLine("", notice);
}

void ProtocolSessionPanel::AppendLine(const wxString& prefix, const wxString& content) {
    if (!m_dataLogTextCtrl) return;

    wxString line;
    if (m_showTimestamp && m_showTimestamp->IsChecked()) {
        line = wxDateTime::Now().Format("[%Y-%m-%d %H:%M:%S] ");
    }
    if (!prefix.IsEmpty()) {
        line += prefix + " ";
    }
    line += content + "\n";

    m_dataLogTextCtrl->AppendText(line);

    const bool shouldScroll = (!m_autoScroll || m_autoScroll->IsChecked());
    if (shouldScroll) {
        m_dataLogTextCtrl->SetInsertionPointEnd();
    }
}

wxString ProtocolSessionPanel::FormatMessage(const std::vector<uint8_t>& msg) {
    if (m_hexDisplay && m_hexDisplay->IsChecked()) {
        wxString text;
        for (uint8_t b : msg) {
            text += wxString::Format("%02X ", b);
        }
        if (!text.IsEmpty()) {
            text.RemoveLast();  // 去尾空格
        }
        return text;
    }
    return wxString::FromUTF8(reinterpret_cast<const char*>(msg.data()), msg.size());
}

void ProtocolSessionPanel::OnMessage(int clientId, const std::vector<uint8_t>& msg) {
    m_rxBytes += msg.size();
    m_rxLabel->SetLabel(wxString::Format(_("Received: %llu bytes"), (unsigned long long)m_rxBytes));

    const wxString text = FormatMessage(msg);
    const wxString prefix = (clientId > 0) ? wxString::Format("<#%d", clientId) : "<";

    AppendLine(prefix, text);
}

void ProtocolSessionPanel::OnSend(wxCommandEvent&) {
    if (!m_session || !m_session->IsConnected()) return;

    const wxString text = m_sendTextCtrl->GetValue();
    if (text.IsEmpty()) return;

    std::string utf8 = std::string(text.utf8_str());
    if (m_appendNewline && m_appendNewline->IsChecked()) {
        utf8 += "\n";
    }
    std::vector<uint8_t> bytes(utf8.begin(), utf8.end());

    if (m_session->Send(bytes)) {
        m_txBytes += bytes.size();
        m_txLabel->SetLabel(wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));
        AppendLine(">", text);
        m_sendTextCtrl->Clear();
    } else {
        AppendLine("!", _("Send failed"));
    }
}

void ProtocolSessionPanel::OnSendEnter(wxCommandEvent& evt) {
    OnSend(evt);
}

void ProtocolSessionPanel::OnClear(wxCommandEvent&) {
    if (m_dataLogTextCtrl) {
        m_dataLogTextCtrl->Clear();
    }
}

void ProtocolSessionPanel::OnAction(wxCommandEvent&) {
    if (!m_session) return;

    switch (m_session->State()) {
        case SessionState::Idle:
        case SessionState::Disconnected:
            SPDLOG_INFO(_("User clicked Open Connection"));
            if (m_onRequestOpen) {
                m_onRequestOpen();
            } else {
                m_session->Open();
                RefreshFromSession();
            }
            break;

        case SessionState::Connecting:
        case SessionState::Reconnecting:
            SPDLOG_INFO(_("User cancelled connecting/reconnecting"));
            m_session->Close();
            RefreshFromSession();
            break;

        case SessionState::Connected:
            SPDLOG_INFO(_("User clicked Disconnect"));
            m_session->Close();
            RefreshFromSession();
            break;
    }
}

void ProtocolSessionPanel::UpdateActionButton(SessionState st) {
    const bool isServer = m_session && m_session->IsServerMode();

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
            if (isServer)
                m_actionBtn->SetLabel(_("Stop Listening"));
            else
                m_actionBtn->SetLabel(_("Disconnect"));
            break;
    }
    m_actionBtn->Enable(true);

    const bool canConfig = (st == SessionState::Idle || st == SessionState::Disconnected);
    m_configBtn->Enable(canConfig);

    const bool canSend = (st == SessionState::Connected);
    m_sendBtn->Enable(canSend);
    m_sendTextCtrl->Enable(canSend);
}

void ProtocolSessionPanel::OnReconnectAttempt(int attempt) {
    AppendLine("", wxString::Format(_("Reconnecting (attempt %d) ..."), attempt));
}

void ProtocolSessionPanel::OnConfigure(wxCommandEvent&) {
    SPDLOG_INFO(_("User clicked Reconfigure"));
    if (m_onRequestConfigure) {
        m_onRequestConfigure();
    }
}

void ProtocolSessionPanel::UpdateClientsLabel() {
    if (!m_clientsLabel) return;
    m_clientsLabel->SetLabel(wxString::Format(_("Clients: %d"), m_clientCount));
}

void ProtocolSessionPanel::OnClientConnected(int clientId) {
    ++m_clientCount;
    UpdateClientsLabel();

    if (m_session) UpdateStateLabel(m_session->State());

    AppendLine("", wxString::Format(_("Client #%d connected"), clientId));
}

void ProtocolSessionPanel::OnClientDisconnected(int clientId) {
    if (m_clientCount > 0) --m_clientCount;
    UpdateClientsLabel();

    if (m_session) UpdateStateLabel(m_session->State());

    AppendLine("", wxString::Format(_("Client #%d disconnected"), clientId));
}

void ProtocolSessionPanel::UpdateModeLabel() {
    if (!m_session || !m_modeLabel) return;

    const bool isServer = m_session->IsServerMode();
    wxString transport = wxString::FromUTF8(m_session->TransportName().c_str());

    if (transport.EndsWith("-Server")) transport = transport.Left(transport.Length() - 7);

    wxString mode = isServer ? _("Server Mode") : _("Client Mode");

    m_modeLabel->SetLabel(wxString::Format("%s  %s", transport, mode));
}

}  // namespace icu
