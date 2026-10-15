#pragma once

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <functional>

#include "core/Session.h"

namespace icu {

class ProtocolSessionPanel : public wxPanel {
public:
    ProtocolSessionPanel(wxWindow* parent, Session* session);
    ~ProtocolSessionPanel() override;

    void RefreshFromSession();
    void OnMessage(int clientId, const std::vector<uint8_t>& msg);
    void OnReconnectAttempt(int attempt);

    using RequestCallback = std::function<void()>;
    void OnRequestOpen(RequestCallback cb) { m_onRequestOpen = std::move(cb); }
    void OnRequestConfigure(RequestCallback cb) { m_onRequestConfigure = std::move(cb); }

    void OnClientConnected(int clientId);
    void OnClientDisconnected(int clientId);

private:
    Session* m_session = nullptr;
    SessionState m_lastState = SessionState::Idle;

    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_targetLabel = nullptr;
    wxStaticText* m_rxLabel = nullptr;
    wxStaticText* m_txLabel = nullptr;

    wxTextCtrl* m_dataLogTextCtrl = nullptr;
    wxTextCtrl* m_sendTextCtrl = nullptr;
    wxButton* m_sendBtn = nullptr;
    wxButton* m_actionBtn = nullptr;
    wxButton* m_configBtn = nullptr;

    // 阶段 5.5：通用显示增强
    wxCheckBox* m_hexDisplay = nullptr;     // HEX 接收显示
    wxCheckBox* m_showTimestamp = nullptr;  // 时间戳
    wxCheckBox* m_autoScroll = nullptr;     // 自动滚动
    wxCheckBox* m_appendNewline = nullptr;  // 发送时追加 \n
    wxButton* m_clearBtn = nullptr;         // 清空接收

    size_t m_rxBytes = 0;
    size_t m_txBytes = 0;

    RequestCallback m_onRequestOpen;
    RequestCallback m_onRequestConfigure;

    wxStaticText* m_modeLabel = nullptr;

    void BuildUi();
    void AppendLine(const wxString& prefix, const wxString& content);
    void AppendNotice(SessionState oldSt, SessionState newSt);
    void UpdateStateLabel(SessionState st);
    void UpdateActionButton(SessionState st);

    // 根据 m_hexDisplay 决定如何格式化消息
    wxString FormatMessage(const std::vector<uint8_t>& msg);

    void OnSend(wxCommandEvent& evt);
    void OnSendEnter(wxCommandEvent& evt);
    void OnAction(wxCommandEvent& evt);
    void OnConfigure(wxCommandEvent& evt);
    void OnClear(wxCommandEvent& evt);

    int m_clientCount = 0;
    wxStaticText* m_clientsLabel = nullptr;
    void UpdateClientsLabel();

    void UpdateModeLabel();
};

}  // namespace icu
