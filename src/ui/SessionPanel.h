#pragma once

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <functional>
#include <map>
#include <nlohmann/json.hpp>

#include "core/IProtocol.h"
#include "core/ISession.h"
#include "core/ITransactionSession.h"
#include "core/SessionConfig.h"
#include "core/StreamSession.h"
#include "core/TestRunner.h"

namespace icu {

class SessionPanel : public wxPanel {
public:
    SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session,
                 SessionConfig* sessionConfig);
    ~SessionPanel() override;

    using RequestCallback = std::function<void()>;
    void OnRequestOpen(RequestCallback cb) { m_onRequestOpen = std::move(cb); }
    void OnRequestConfigure(RequestCallback cb) { m_onRequestConfigure = std::move(cb); }
    void RefreshFromSession();
    void AppendTxLog(const wxString& line);

    using StateCallback = std::function<void(SessionState)>;
    void OnStateUpdated(StateCallback cb) { m_onStateUpdated = std::move(cb); }

private:
    IProtocol* m_protocol = nullptr;
    int m_modeIndex = 0;
    ISession* m_session = nullptr;
    SessionConfig* m_sessionConfig = nullptr;

    StreamSession* m_streamSession = nullptr;
    ITransactionSession* m_txSession = nullptr;

    SessionState m_lastState = SessionState::Idle;

    // --- 主区域选项卡 ---
    wxNotebook* m_modeNotebook = nullptr;
    wxPanel* m_manualPage = nullptr;
    wxPanel* m_interactivePage = nullptr;
    wxPanel* m_autoPage = nullptr;

    // --- 测试流组件（交互/自动模式） ---
    wxButton* m_runBtn = nullptr;
    wxButton* m_stopBtn = nullptr;
    TestRunner* m_testRunner = nullptr;

    // --- 通信变量（全局） ---
    wxListCtrl* m_varList = nullptr;
    wxButton* m_addVarBtn = nullptr;
    wxButton* m_editVarBtn = nullptr;
    wxButton* m_delVarBtn = nullptr;

    // --- 变量操作区（动态） ---
    wxStaticBoxSizer* m_varOpSizer = nullptr;
    std::map<std::string, wxWindow*> m_varOpControls;
    wxButton* m_executeReadBtn = nullptr;
    wxButton* m_executeWriteBtn = nullptr;
    std::string m_selectedVarName;

    // --- 公共组件 ---
    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_targetLabel = nullptr;
    wxButton* m_actionBtn = nullptr;
    wxButton* m_configBtn = nullptr;

    // --- 字节流专属 ---
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

    // --- 事务型专属 ---
    wxTextCtrl* m_txLogTextCtrl = nullptr;

    RequestCallback m_onRequestOpen;
    RequestCallback m_onRequestConfigure;
    StateCallback m_onStateUpdated;

    void BuildUi();
    void BuildCommonUi(wxBoxSizer* mainSizer);
    void BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);
    void BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);
    void BuildVariableUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);
    void BuildTestFlowUi(wxPanel* parent, bool isAutoMode);

    wxWindow* CreateControl(wxWindow* parent, const FieldDescriptor& fd);
    void UpdateStateLabel(SessionState st);
    void UpdateActionButton(SessionState st);
    void UpdateClientsLabel();

    void AppendLine(const wxString& prefix, const wxString& content);
    wxString FormatMessage(const std::vector<uint8_t>& msg);
    void OnSend(wxCommandEvent& evt);
    void OnSendEnter(wxCommandEvent& evt);
    void OnClear(wxCommandEvent& evt);
    void OnAction(wxCommandEvent& evt);
    void OnConfigure(wxCommandEvent& evt);
    void OnExecuteRead(wxCommandEvent& evt);
    void OnExecuteWrite(wxCommandEvent& evt);

    void BindStreamCallbacks();
    void BindTransactionCallbacks();

    void OnNotebookPageChanged(wxBookCtrlEvent& evt);
    void OnSendTextChanged(wxCommandEvent& evt);

    void RefreshVariableList();
    void OnVarSelected(wxListEvent& evt);
    void OnAddVariable(wxCommandEvent& evt);
    void OnEditVariable(wxCommandEvent& evt);
    void OnDeleteVariable(wxCommandEvent& evt);
    void BuildVarOpUi(const CommVariable& var);
};

}  // namespace icu
