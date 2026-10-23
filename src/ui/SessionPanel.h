#pragma once

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <set>

#include "core/Database.h"
#include "core/IProtocol.h"
#include "core/ISession.h"
#include "core/ITransactionSession.h"
#include "core/SessionConfig.h"
#include "core/StreamSession.h"
#include "core/TestRunner.h"
#include "protocols/mqtt/MqttSession.h"
#include "ui/ChartPanel.h"
#include "ui/HistoryDialog.h"

namespace icu {

class SessionPanel : public wxPanel {
public:
    SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session,
                 SessionConfig* sessionConfig, const wxString& profilePath);
    ~SessionPanel() override;

    using RequestCallback = std::function<void()>;
    void OnRequestOpen(RequestCallback cb) { m_onRequestOpen = std::move(cb); }
    void OnRequestConfigure(RequestCallback cb) { m_onRequestConfigure = std::move(cb); }
    void RefreshFromSession();

    // 从全局设置读取并应用
    void ApplyGlobalSettings();

    using StateCallback = std::function<void(SessionState)>;
    void OnStateUpdated(StateCallback cb) { m_onStateUpdated = std::move(cb); }

private:
    IProtocol* m_protocol = nullptr;
    int m_modeIndex = 0;
    ISession* m_session = nullptr;
    SessionConfig* m_sessionConfig = nullptr;
    wxString m_profilePath;

    StreamSession* m_streamSession = nullptr;
    ITransactionSession* m_txSession = nullptr;
    MqttSession* m_mqttSession = nullptr;

    SessionState m_lastState = SessionState::Idle;

    Database m_db;
    int64_t m_currentRunId = -1;
    ChartPanel* m_chartPanel = nullptr;

    std::set<std::string> m_loggingVars;
    bool m_isRecording = false;
    wxButton* m_recordBtn = nullptr;
    wxButton* m_historyBtn = nullptr;

    void RecordData(const std::string& varName, double value);
    void AppendUnifiedLog(bool isRx, const std::string& varName, const std::string& type,
                          const std::string& value, const std::vector<uint8_t>& rawBytes,
                          int clientId = 0);

    wxSplitterWindow* m_mainSplitter = nullptr;
    wxSplitterWindow* m_rightSplitter = nullptr;
    wxNotebook* m_modeNotebook = nullptr;
    wxPanel* m_manualPage = nullptr;
    wxPanel* m_interactivePage = nullptr;
    wxPanel* m_autoPage = nullptr;

    wxButton* m_runBtn = nullptr;
    wxButton* m_stopBtn = nullptr;
    TestRunner* m_testRunner = nullptr;

    wxListCtrl* m_varList = nullptr;
    wxButton* m_addVarBtn = nullptr;
    wxButton* m_editVarBtn = nullptr;
    wxButton* m_delVarBtn = nullptr;
    wxButton* m_copyVarBtn = nullptr;

    wxStaticBoxSizer* m_varOpSizer = nullptr;
    std::map<std::string, wxWindow*> m_varOpControls;
    wxButton* m_executeReadBtn = nullptr;
    wxButton* m_executeWriteBtn = nullptr;
    std::string m_selectedVarName;

    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_targetLabel = nullptr;
    wxButton* m_actionBtn = nullptr;
    wxButton* m_configBtn = nullptr;

    wxTextCtrl* m_commonLogCtrl = nullptr;
    wxButton* m_clearBtn = nullptr;

    wxStaticText* m_rxLabel = nullptr;
    wxStaticText* m_txLabel = nullptr;
    wxStaticText* m_clientsLabel = nullptr;
    size_t m_rxBytes = 0;
    size_t m_txBytes = 0;
    int m_clientCount = 0;

    wxCheckBox* m_hexDisplay = nullptr;
    wxCheckBox* m_showTimestamp = nullptr;
    wxCheckBox* m_autoScroll = nullptr;

    RequestCallback m_onRequestOpen;
    RequestCallback m_onRequestConfigure;
    StateCallback m_onStateUpdated;

    void BuildUi();
    void BuildCommonUi(wxBoxSizer* mainSizer);
    void BuildTestFlowUi(wxPanel* parent, bool isAutoMode);
    void BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);
    void BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);

    wxWindow* CreateJsonControl(wxWindow* parent, const FieldDescriptor& fd,
                                const nlohmann::json& currentValue,
                                std::function<void(const nlohmann::json&)> onChanged);

    void UpdateStateLabel(SessionState st);
    void UpdateActionButton(SessionState st);
    void UpdateClientsLabel();

    void OnClear(wxCommandEvent& evt);
    void OnAction(wxCommandEvent& evt);
    void OnConfigure(wxCommandEvent& evt);
    void OnExecuteRead(wxCommandEvent& evt);
    void OnExecuteWrite(wxCommandEvent& evt);
    void OnExecuteSend(wxCommandEvent& evt);

    void BindStreamCallbacks();
    void BindTransactionCallbacks();
    void BindMqttCallbacks();

    void OnNotebookPageChanged(wxBookCtrlEvent& evt);

    void RefreshVariableList();
    void OnVarSelected(wxListEvent& evt);
    void OnItemChecked(wxListEvent& evt);
    void OnAddVariable(wxCommandEvent& evt);
    void OnCopyVariable(wxCommandEvent& evt);
    void OnEditVariable(wxCommandEvent& evt);
    void OnDeleteVariable(wxCommandEvent& evt);
    void BuildVarOpUi(const CommVariable& var);

    void OnRecordSettings(wxCommandEvent& evt);
    void OnHistoryQuery(wxCommandEvent& evt);
};

}  // namespace icu
