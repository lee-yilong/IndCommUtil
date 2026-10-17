#pragma once

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <functional>
#include <map>

#include "core/IProtocol.h"
#include "core/ISession.h"
#include "core/ITransactionSession.h"
#include "core/StreamSession.h"

namespace icu {

class SessionPanel : public wxPanel {
public:
    SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session);
    ~SessionPanel() override;

    // 供 MainFrame 调用的接口
    using RequestCallback = std::function<void()>;
    void OnRequestOpen(RequestCallback cb) { m_onRequestOpen = std::move(cb); }
    void OnRequestConfigure(RequestCallback cb) { m_onRequestConfigure = std::move(cb); }

    // 如果 MainFrame 需要知道状态更新，可提供，但 SessionPanel 内部会自己处理状态栏
    void RefreshFromSession();

    using StateCallback = std::function<void(SessionState)>;
    void OnStateUpdated(StateCallback cb) { m_onStateUpdated = std::move(cb); }

    void OnFunctionCodeChanged(wxCommandEvent& evt);
    void UpdateOperationUiState();

private:
    StateCallback m_onStateUpdated;
    IProtocol* m_protocol = nullptr;
    int m_modeIndex = 0;
    ISession* m_session = nullptr;

    // 具体类型的强转指针（可能为空）
    StreamSession* m_streamSession = nullptr;
    ITransactionSession* m_txSession = nullptr;

    SessionState m_lastState = SessionState::Idle;

    // --- 公共组件 ---
    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_targetLabel = nullptr;
    wxButton* m_actionBtn = nullptr;
    wxButton* m_configBtn = nullptr;

    // --- 字节流专属组件 ---
    wxTextCtrl* m_dataLogTextCtrl = nullptr;
    wxTextCtrl* m_sendTextCtrl = nullptr;
    wxButton* m_sendBtn = nullptr;
    wxCheckBox* m_hexDisplay = nullptr;
    wxCheckBox* m_showTimestamp = nullptr;
    wxCheckBox* m_autoScroll = nullptr;
    wxCheckBox* m_appendNewline = nullptr;
    wxButton* m_clearBtn = nullptr;
    wxStaticText* m_rxLabel = nullptr;
    wxStaticText* m_txLabel = nullptr;
    wxStaticText* m_clientsLabel = nullptr;
    size_t m_rxBytes = 0;
    size_t m_txBytes = 0;
    int m_clientCount = 0;

    // --- 事务型专属组件 ---
    std::map<std::string, wxWindow*> m_operationControls;
    wxTextCtrl* m_txLogTextCtrl = nullptr;
    wxButton* m_executeBtn = nullptr;

    RequestCallback m_onRequestOpen;
    RequestCallback m_onRequestConfigure;

    void BuildUi();
    void BuildCommonUi(wxBoxSizer* mainSizer);
    void BuildStreamUi(wxBoxSizer* mainSizer);
    void BuildTransactionUi(wxBoxSizer* mainSizer);

    // 动态创建表单控件
    wxWindow* CreateControl(wxWindow* parent, const FieldDescriptor& fd);

    void UpdateStateLabel(SessionState st);
    void UpdateActionButton(SessionState st);
    void UpdateClientsLabel();

    // --- 字节流事件 ---
    void AppendLine(const wxString& prefix, const wxString& content);
    wxString FormatMessage(const std::vector<uint8_t>& msg);
    void OnSend(wxCommandEvent& evt);
    void OnSendEnter(wxCommandEvent& evt);
    void OnClear(wxCommandEvent& evt);

    // --- 事务型事件 ---
    void AppendTxLog(const wxString& line);
    void OnExecute(wxCommandEvent& evt);

    // --- 公共事件 ---
    void OnAction(wxCommandEvent& evt);
    void OnConfigure(wxCommandEvent& evt);

    // --- 底层回调绑定 ---
    void BindStreamCallbacks();
    void BindTransactionCallbacks();
};

}  // namespace icu
