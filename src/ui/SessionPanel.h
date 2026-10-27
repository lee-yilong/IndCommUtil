#pragma once

#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/statbmp.h>
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
#include "ui/UIUtils.h"

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

    void ApplyGlobalSettings();

    using StateCallback = std::function<void(SessionState)>;
    void OnStateUpdated(StateCallback cb) { m_onStateUpdated = std::move(cb); }

    void CmdAdd();
    void CmdCopy();
    void CmdEdit();
    void CmdDelete();

    bool HasSelection() const;

private:
    enum class ActiveContext { Variables, Steps };
    ActiveContext m_activeContext = ActiveContext::Variables;

    enum class ActionIcon { Open, Cancel, Disconnect };
    ActionIcon m_currentActionIcon = ActionIcon::Open;

    enum class RunStopIcon { Run, Stop };
    RunStopIcon m_currentRunStopIcon = RunStopIcon::Run;

    enum class RecordIcon { Record, StopRecording };
    RecordIcon m_currentRecordIcon = RecordIcon::Record;

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
    std::string m_historyKey;
    ChartPanel* m_chartPanel = nullptr;

    std::set<std::string> m_loggingVars;
    bool m_isRecording = false;

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

    // ★ 原生 wxBitmapButton 工具栏：直接响应点击，不做 UPDATE_UI 轮询。
    //   这里缓存每个按钮当前显示的图标 / 可用状态 / 提示，只有真正变化时才刷新控件，
    //   避免每帧重复解码 SVG 与 SetBitmap 造成的卡顿。
    struct ToolBtnState {
        wxBitmapButton* btn = nullptr;
        wxString iconKey;  // 当前已绘制的图标名，用于避免重复解码 SVG
        bool enabled = true;
        wxString tooltip;
    };
    std::map<int, ToolBtnState> m_toolBtnMap;

    TestRunner* m_testRunner = nullptr;

    wxListCtrl* m_varList = nullptr;
    wxBoxSizer* m_varOpSizer = nullptr;
    std::map<std::string, wxWindow*> m_varOpControls;
    std::string m_selectedVarName;
    // 重建变量列表时抑制 wxEVT_LIST_ITEM_SELECTED，避免重建右侧操作区
    bool m_refreshingVarList = false;

    // 手动操作区的按钮与校验错误提示
    ui::FlatButton* m_sendBtn = nullptr;
    ui::FlatButton* m_readBtn = nullptr;
    ui::FlatButton* m_writeBtn = nullptr;
    wxStaticText* m_valueHintLabel = nullptr;

    // 列表拖拽排序的临时状态
    wxListCtrl* m_dragList = nullptr;
    long m_dragIndex = -1;
    bool m_dragging = false;
    wxPoint m_dragStart;

    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_targetLabel = nullptr;
    wxStaticText* m_rxLabel = nullptr;
    wxStaticText* m_txLabel = nullptr;
    wxStaticText* m_clientsLabel = nullptr;
    size_t m_rxBytes = 0;
    size_t m_txBytes = 0;
    int m_clientCount = 0;

    wxTextCtrl* m_commonLogCtrl = nullptr;

    wxCheckBox* m_hexDisplay = nullptr;
    wxCheckBox* m_showTimestamp = nullptr;
    wxCheckBox* m_autoScroll = nullptr;

    wxListCtrl* m_interactiveStepList = nullptr;
    wxListCtrl* m_autoStepList = nullptr;

    RequestCallback m_onRequestOpen;
    RequestCallback m_onRequestConfigure;
    StateCallback m_onStateUpdated;

    void BuildUi();
    void BuildCommonUi(wxBoxSizer* mainSizer);
    void BuildTestFlowUi(wxPanel* parent, bool isAutoMode);
    void BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);
    void BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow);

    // ★ 创建原生工具栏按钮，并在内部直接绑定点击事件
    wxBitmapButton* CreateToolBtn(wxWindow* parent, int id, const wxString& iconName,
                                  const wxString& tooltip);
    // ★ 分发工具栏按钮的点击事件
    void OnToolClick(wxCommandEvent& evt);
    // ★ 按当前会话 / 选中状态显式刷新工具栏（只在状态变化时真正刷新控件）
    void UpdateToolbarState();
    // ★ 设置单个按钮的图标 / 可用状态 / 提示，内部做差异比较
    void SetToolBtn(int id, const wxString& iconName, bool enabled, const wxString& tooltip);

    wxWindow* CreateJsonControl(wxWindow* parent, const FieldDescriptor& fd,
                                const nlohmann::json& currentValue,
                                std::function<void(const nlohmann::json&)> onChanged);

    void UpdateStateLabel(SessionState st);
    void UpdateActionButton(SessionState st);
    void UpdateClientsLabel();

    void OnTbAction(wxCommandEvent& evt);
    void OnTbConfigure(wxCommandEvent& evt);
    void OnTbRecord(wxCommandEvent& evt);
    void OnTbHistory(wxCommandEvent& evt);
    void OnTbAdd(wxCommandEvent& evt);
    void OnTbCopy(wxCommandEvent& evt);
    void OnTbEdit(wxCommandEvent& evt);
    void OnTbDelete(wxCommandEvent& evt);
    void OnTbRunStop(wxCommandEvent& evt);
    void OnTbBatch(wxCommandEvent& evt);
    void OnTbUp(wxCommandEvent& evt);
    void OnTbDown(wxCommandEvent& evt);

    void OnClear(wxCommandEvent& evt);
    void OnExecuteRead(wxCommandEvent& evt);
    void OnExecuteWrite(wxCommandEvent& evt);
    void OnExecuteSend(wxCommandEvent& evt);

    // 执行手动操作前校验当前变量的值；不合法时弹提示并返回 false
    bool ValidateSelectedValue(const wxString& action);

    void BindStreamCallbacks();
    void BindTransactionCallbacks();
    void BindMqttCallbacks();

    void OnNotebookPageChanged(wxBookCtrlEvent& evt);

    void RefreshVariableList();
    void RefreshStepList(wxListCtrl* stepList, const nlohmann::json& flow);
    void OnVarSelected(wxListEvent& evt);

    // 变量值类型校验：更新提示文字并启/禁用发送按钮
    void ValidateValueInput();
    // 变量 / 步骤列表的右键菜单
    void ShowVarContextMenu(const wxPoint& pos);
    void ShowStepContextMenu(wxListCtrl* stepList, const wxPoint& pos);
    // 列表拖拽排序
    void OnListLeftDown(wxListCtrl* list, const wxPoint& pos);
    void OnListMotion(wxListCtrl* list, const wxPoint& pos);
    void OnListLeftUp(wxListCtrl* list, const wxPoint& pos);
    void CancelListDrag();
    void MoveVariableOrder(long from, long to);
    void MoveStepOrder(wxListCtrl* stepList, nlohmann::json& flow, long from, long to);
    // 测试步骤失败时询问用户：重试 / 跳过 / 结束
    StepFailureAction AskStepFailure(int stepIndex, const std::string& message);

    void DoAddVariable();
    void DoCopyVariable();
    void DoEditVariable();
    void DoDeleteVariable();

    void DoAddStep();
    void DoCopyStep();
    void DoEditStep();
    void DoDeleteStep();
    void DoBatchStep();
    void DoUpStep();
    void DoDownStep();

    void BuildVarOpUi(const CommVariable& var);
};

}  // namespace icu
