#include "SessionPanel.h"

#include <wx/artprov.h>
#include <wx/bmpbndl.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/mousestate.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#include "TestStepEditDialog.h"
#include "VariableEditDialog.h"
#include "core/BytesParser.h"
#include "core/DelayStep.h"
#include "core/ValueValidator.h"
#include "log/Log.h"
#include "settings/UISettings.h"
#include "spdlog/spdlog.h"
#include "ui/UIUtils.h"

namespace icu {

namespace {

const wxColour kThemeColor(56, 189, 248);
const wxColour kThemeDark(30, 41, 59);
const wxColour kBgColor(243, 244, 246);
const wxColour kCardBgColor(255, 255, 255);
const wxColour kTextColor(51, 65, 85);
const wxColour kSubTextColor(100, 116, 139);
const wxColour kBorderColor(229, 231, 235);
const wxColour kIconTint(51, 65, 85);

wxPanel* CreateCardSection(wxWindow* parent, const wxString& title, wxBoxSizer** outContentSizer) {
    auto* card = new ui::CardPanel(parent);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* titleLabel = new wxStaticText(card, wxID_ANY, title);
    wxFont tf = titleLabel->GetFont();
    tf.SetPointSize(10);
    tf.MakeBold();
    titleLabel->SetFont(tf);
    titleLabel->SetForegroundColour(kSubTextColor);
    mainSizer->Add(titleLabel, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto* contentSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(contentSizer, 1, wxEXPAND | wxALL, 8);

    card->SetSizer(mainSizer);
    if (outContentSizer) *outContentSizer = contentSizer;
    return card;
}

bool TryParseNumeric(const std::string& type, const std::string& val, double& out) {
    try {
        if (type == "hex") {
            std::string s = val;
            s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
            if (s.empty()) return false;
            out = static_cast<double>(std::stoul(s, nullptr, 16));
            return true;
        } else if (type != "string" && type != "bool") {
            out = std::stod(val);
            return true;
        }
    } catch (...) {
    }
    return false;
}

wxString FormatDurationCN(long long milliseconds) {
    if (milliseconds < 0) milliseconds = 0;
    const long long totalSec = (milliseconds + 500) / 1000;
    if (totalSec >= 3600) {
        return wxString::Format("%02lld%s%02lld%s%02lld%s", totalSec / 3600, _("h"),
                                (totalSec % 3600) / 60, _("min"), totalSec % 60, _("s"));
    }
    return wxString::Format("%02lld%s%02lld%s", totalSec / 60, _("min"), totalSec % 60, _("s"));
}

int ShowMsg(const wxString& message, const wxString& title, long style,
            wxWindow* parent = nullptr) {
    if (!parent && wxTheApp) parent = wxTheApp->GetTopWindow();
    return ui::MessageDialog::Show(parent, title, message, style);
}

wxPoint ContextMenuPos(wxWindow* window, const wxPoint& eventPosition) {
    if (!window) return eventPosition;
    const wxPoint mouse = wxGetMousePosition();
    const wxRect rect(window->ClientToScreen(wxPoint(0, 0)), window->GetClientSize());
    if (rect.Contains(mouse)) return window->ScreenToClient(mouse);
    return eventPosition;
}

wxString DescribeStep(const nlohmann::json& step) {
    const std::string action = step.value("action", "");
    const int waitMs = step.value("wait_ms", 0);

    if (action.empty()) return {};
    if (action == "delay") return wxString::Format("%s %dms", _("Delay"), waitMs);

    wxString actionLabel = wxString::FromUTF8(action);
    if (action == "write")
        actionLabel = _("Write");
    else if (action == "read")
        actionLabel = _("Read");

    wxString text = actionLabel;
    const std::string variable = step.value("variable", "");
    if (!variable.empty()) text += " " + wxString::FromUTF8(variable);

    if (action == "write" && step.contains("value")) {
        const auto& value = step["value"];
        text += " = " + (value.is_string() ? wxString::FromUTF8(value.get<std::string>())
                                           : wxString::FromUTF8(value.dump()));
    }
    if (waitMs > 0) text += wxString::Format("  |  %s %dms", _("Wait"), waitMs);

    if (step.contains("assert") && step["assert"].is_object()) {
        const auto& a = step["assert"];
        const std::string type = a.value("type", "");
        const std::string expected = a.value("expected", "");
        if (!type.empty())
            text += wxString::Format("  |  %s %s %s", _("Assert"), wxString::FromUTF8(type),
                                     wxString::FromUTF8(expected));
    }
    return text;
}

enum {
    idTbAction = 5000,
    idTbConfigure,
    idTbRecord,
    idTbHistory,
    idTbAdd,
    idTbCopy,
    idTbEdit,
    idTbDelete,
    idTbRunStop,
    idTbBatch,
    idTbUp,
    idTbDown
};

const int kTimeUnitMultipliers[] = {1, 60, 3600, 86400};
constexpr int kTimeUnitCount = 4;
}  // namespace

SessionPanel::SessionPanel(wxWindow* parent, IProtocol* protocol, int modeIndex, ISession* session,
                           SessionConfig* sessionConfig, const wxString& profilePath)
    : wxPanel(parent, wxID_ANY),
      m_protocol(protocol),
      m_modeIndex(modeIndex),
      m_session(session),
      m_sessionConfig(sessionConfig),
      m_profilePath(profilePath) {
    m_streamSession = dynamic_cast<StreamSession*>(m_session);
    m_txSession = dynamic_cast<ITransactionSession*>(m_session);
    m_mqttSession = dynamic_cast<MqttSession*>(m_session);

    SetBackgroundColour(kBgColor);

    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    exePath.SetFullName("test_history.db");
    m_db.Open(std::string(exePath.GetFullPath().utf8_str()));

    wxString runPath = m_profilePath.IsEmpty() ? "Untitled" : m_profilePath;
    m_historyKey = std::string(runPath.utf8_str());
    m_currentRunId = m_db.CreateTestRun(m_historyKey);

    BuildUi();
    CallAfter([this]() {
        if (m_mainSplitter) m_mainSplitter->SetSashPosition(360);
    });

    m_runTimer = new wxTimer(this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
        if (m_runTimingActive) UpdateRunTiming(m_activeStepList);
    });

    if (m_streamSession)
        BindStreamCallbacks();
    else if (m_txSession)
        BindTransactionCallbacks();
    else if (m_mqttSession)
        BindMqttCallbacks();

    if (m_session) {
        m_lastState = m_session->State();
        UpdateStateLabel(m_lastState);
        UpdateActionButton(m_lastState);
        RefreshFromSession();
    }

    if (m_sessionConfig && m_modeNotebook) {
        m_modeNotebook->SetSelection(static_cast<int>(m_sessionConfig->activeMode));
    }

    // 首次按真实状态同步工具栏（不依赖 wxEVT_UPDATE_UI）
    UpdateToolbarState();
}

SessionPanel::~SessionPanel() {
    if (m_runTimer) {
        m_runTimer->Stop();
        delete m_runTimer;
        m_runTimer = nullptr;
    }
    if (m_pendingFailure) {
        {
            std::lock_guard<std::mutex> lock(m_pendingFailure->mutex);
            m_pendingFailure->action = StepFailureAction::Stop;
            m_pendingFailure->done = true;
        }
        m_pendingFailure->cv.notify_all();
        m_pendingFailure.reset();
    }
    if (m_testRunner) {
        m_testRunner->Stop();
        delete m_testRunner;
        m_testRunner = nullptr;
    }
    if (m_currentRunId != -1) m_db.UpdateTestRunStatus(m_currentRunId, "CLOSED");
    m_db.Close();
}

void SessionPanel::ApplyGlobalSettings() {
    auto& ui = settings::UISettings::Instance();
    if (m_commonLogCtrl) {
        m_commonLogCtrl->SetShowHex(ui.GetHexDisplay());
        m_commonLogCtrl->SetShowTime(ui.GetShowTimestamp());
        m_commonLogCtrl->SetAutoScroll(ui.GetAutoScroll());
    }
}

void SessionPanel::RecordData(const std::string& varName, double value) {
    if (!m_isRecording) return;
    if (!m_loggingVars.empty() && m_loggingVars.find(varName) == m_loggingVars.end()) return;

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    double timeSec = ms / 1000.0;

    // 构造会话时数据库可能被其它连接短暂占用；真正写入时再补一次
    if (m_currentRunId == -1) m_currentRunId = m_db.CreateTestRun(m_historyKey);
    if (m_currentRunId != -1) m_db.InsertRunData(m_currentRunId, varName, value, timeSec);
    if (m_chartPanel) m_chartPanel->AddDataPointWithTime(varName, value, timeSec);
}

void SessionPanel::RecordVariableValue(const std::string& varName, const std::string& value) {
    if (!m_isRecording || !m_sessionConfig) return;
    if (!m_loggingVars.empty() && m_loggingVars.find(varName) == m_loggingVars.end()) return;

    auto it = m_sessionConfig->variables.find(varName);
    if (it == m_sessionConfig->variables.end()) return;

    double numeric = 0.0;
    if (TryParseNumeric(it->second.type, value, numeric)) {
        RecordData(varName, numeric);
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const double timeSec = static_cast<double>(ms) / 1000.0;
    if (m_currentRunId == -1) m_currentRunId = m_db.CreateTestRun(m_historyKey);
    if (m_currentRunId != -1) m_db.InsertRunTextData(m_currentRunId, varName, value, timeSec);
}

void SessionPanel::AppendUnifiedLog(bool isRx, const std::string& varName, const std::string& type,
                                    const std::string& value, const std::vector<uint8_t>& rawBytes,
                                    int clientId) {
    if (!m_commonLogCtrl) return;

    const wxString timeStr = wxDateTime::Now().Format("%H:%M:%S");
    wxString varStr = wxString::FromUTF8(varName);
    if (varStr.IsEmpty()) varStr = "Unknown";
    const wxString typeStr = wxString::FromUTF8(type);
    const wxString valStr = wxString::FromUTF8(value);

    wxString hexStr;
    for (uint8_t b : rawBytes) hexStr += wxString::Format("%02X ", b);
    hexStr = hexStr.Trim();
    if (!hexStr.IsEmpty()) hexStr = "HEX " + hexStr;

    wxString content;
    if (isRx && clientId > 0)
        content = wxString::Format("#%d  %s", clientId, valStr);
    else if (isRx)
        content = valStr;
    else
        content = wxString::Format("%s = %s", varStr, valStr);

    if (!typeStr.IsEmpty() && typeStr != "Unknown" && !isRx)
        content = wxString::Format("%s (%s) = %s", varStr, typeStr, valStr);

    m_commonLogCtrl->Append(timeStr, isRx ? ui::LogList::Kind::Receive : ui::LogList::Kind::Send,
                            content, hexStr);
}

void SessionPanel::AppendLogMessage(const wxString& text, bool error) {
    if (!m_commonLogCtrl) return;
    m_commonLogCtrl->AppendMessage(wxDateTime::Now().Format("%H:%M:%S"), text, error);
}

void SessionPanel::AppendRunnerLog(const wxString& text) {
    if (!m_commonLogCtrl) return;
    const wxString timeStr = wxDateTime::Now().Format("%H:%M:%S");

    if (text.StartsWith(">>> "))
        m_commonLogCtrl->Append(timeStr, ui::LogList::Kind::Send, text.Mid(4));
    else if (text.StartsWith("=> "))
        m_commonLogCtrl->Append(timeStr, ui::LogList::Kind::Send, text.Mid(3));
    else if (text.StartsWith("<<< "))
        m_commonLogCtrl->Append(timeStr, ui::LogList::Kind::Receive, text.Mid(4));
    else if (text.StartsWith("!!! "))
        m_commonLogCtrl->Append(timeStr, ui::LogList::Kind::Error, text.Mid(4));
    else if (text.StartsWith("^^^ ") || text.StartsWith("--- "))
        m_commonLogCtrl->AppendMessage(timeStr, text.Mid(4));
    else
        m_commonLogCtrl->AppendMessage(timeStr, text);
}

void SessionPanel::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);
    BuildCommonUi(main);

    m_mainSplitter =
        new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    m_mainSplitter->SetBackgroundColour(kBgColor);
    m_mainSplitter->SetSashGravity(0.0);
    m_mainSplitter->SetMinimumPaneSize(220);

    auto* varPanel = new wxPanel(m_mainSplitter);
    varPanel->SetBackgroundColour(kBgColor);
    auto* varSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* varContentSizer = nullptr;
    auto* varCard = CreateCardSection(varPanel, _("Communication Variables"), &varContentSizer);

    m_varList = new wxListCtrl(varCard, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE);
    m_varList->SetBackgroundColour(kCardBgColor);

    m_varList->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 130);
    m_varList->InsertColumn(1, _("Type"), wxLIST_FORMAT_LEFT, 60);
    m_varList->InsertColumn(2, _("Value"), wxLIST_FORMAT_LEFT, 110);

    m_varList->Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        int w = m_varList->GetClientSize().x;
        if (w > 0) {
            int avail = w - 24;
            if (avail < 100) avail = 100;
            m_varList->SetColumnWidth(0, avail * 0.45);
            m_varList->SetColumnWidth(1, avail * 0.20);
            m_varList->SetColumnWidth(2, avail * 0.35);
        }
        evt.Skip();
    });

    m_varList->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& evt) {
        m_activeContext = ActiveContext::Variables;
        int flags;
        long item = m_varList->HitTest(evt.GetPosition(), flags, nullptr);
        if (item != -1 && item == m_varList->GetItemCount() - 1) {
            CallAfter([this]() {
                m_activeContext = ActiveContext::Variables;
                DoAddVariable();
            });
            return;
        }
        OnListLeftDown(m_varList, evt.GetPosition());
        evt.Skip();
    });
    m_varList->Bind(wxEVT_MOTION, [this](wxMouseEvent& evt) {
        OnListMotion(m_varList, evt.GetPosition());
        evt.Skip();
    });
    m_varList->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& evt) {
        OnListLeftUp(m_varList, evt.GetPosition());
        evt.Skip();
    });
    m_varList->Bind(wxEVT_MOUSE_CAPTURE_LOST,
                    [this](wxMouseCaptureLostEvent&) { CancelListDrag(); });

    // 双击编辑；右键上下文菜单
    m_varList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& evt) {
        long item = evt.GetIndex();
        if (item == m_varList->GetItemCount() - 1) {
            DoAddVariable();
            return;
        }
        m_activeContext = ActiveContext::Variables;
        DoEditVariable();
    });
    m_varList->Bind(wxEVT_CONTEXT_MENU,
                    [this](wxContextMenuEvent& evt) { ShowVarContextMenu(evt.GetPosition()); });

    m_varList->Bind(wxEVT_LIST_ITEM_SELECTED, &SessionPanel::OnVarSelected, this);
    m_varList->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) { UpdateToolbarState(); });
    m_varList->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& evt) {
        if (evt.ControlDown()) {
            if (evt.GetKeyCode() == 'N')
                DoAddVariable();
            else if (evt.GetKeyCode() == 'C' || evt.GetKeyCode() == 'V')
                DoCopyVariable();
            else if (evt.GetKeyCode() == 'E')
                DoEditVariable();
            else
                evt.Skip();
        } else if (evt.GetKeyCode() == WXK_DELETE)
            DoDeleteVariable();
        else
            evt.Skip();
    });

    varContentSizer->Add(m_varList, 1, wxEXPAND);
    varSizer->Add(varCard, 1, wxEXPAND | wxALL, 6);
    varPanel->SetSizer(varSizer);
    RefreshVariableList();

    m_rightSplitter = new wxSplitterWindow(m_mainSplitter, wxID_ANY, wxDefaultPosition,
                                           wxDefaultSize, wxSP_LIVE_UPDATE);
    m_rightSplitter->SetBackgroundColour(kBgColor);
    m_rightSplitter->SetSashGravity(0.7);

    m_modeNotebook = new wxNotebook(m_rightSplitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxNB_TOP | wxBORDER_NONE);
    m_modeNotebook->SetBackgroundColour(kBgColor);

    m_manualPage = new wxPanel(m_modeNotebook);
    m_interactivePage = new wxPanel(m_modeNotebook);
    m_autoPage = new wxPanel(m_modeNotebook);

    m_manualPage->SetBackgroundColour(kBgColor);
    m_interactivePage->SetBackgroundColour(kBgColor);
    m_autoPage->SetBackgroundColour(kBgColor);

    auto* chartPage = new wxPanel(m_modeNotebook);
    chartPage->SetBackgroundColour(kBgColor);
    m_chartPanel = new ChartPanel(chartPage);

    auto* chartSizer = new wxBoxSizer(wxVERTICAL);
    auto* chartBar = new wxBoxSizer(wxHORIZONTAL);

    auto* windowTitle = new wxStaticText(chartPage, wxID_ANY, _("Time Window:"));
    windowTitle->SetForegroundColour(kSubTextColor);
    m_chartWindowValue =
        new wxSpinCtrl(chartPage, wxID_ANY, "10", wxDefaultPosition, wxSize(84, -1),
                       wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 1, 100000, 10);
    m_chartWindowUnit = new wxChoice(chartPage, wxID_ANY);
    m_chartWindowUnit->Append(_("Seconds"));
    m_chartWindowUnit->Append(_("Minutes"));
    m_chartWindowUnit->Append(_("Hours"));
    m_chartWindowUnit->Append(_("Days"));
    m_chartWindowUnit->SetSelection(0);

    auto applyChartWindow = [this](wxEvent&) {
        const int value = m_chartWindowValue->GetValue();
        const int unit = m_chartWindowUnit->GetSelection();
        if (unit < 0 || unit >= kTimeUnitCount || !m_chartPanel) return;
        m_chartPanel->SetTimeWindow(static_cast<double>(value) * kTimeUnitMultipliers[unit]);
    };
    m_chartWindowValue->Bind(wxEVT_SPINCTRL, applyChartWindow);
    m_chartWindowValue->Bind(wxEVT_TEXT_ENTER, applyChartWindow);
    m_chartWindowValue->Bind(wxEVT_KILL_FOCUS, applyChartWindow);
    m_chartWindowUnit->Bind(wxEVT_CHOICE, applyChartWindow);

    auto* chartHint = new wxStaticText(chartPage, wxID_ANY,
                                       _("Scroll to zoom | Drag to pan | Double-click to follow"));
    chartHint->SetForegroundColour(kSubTextColor);

    chartBar->Add(windowTitle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    chartBar->Add(m_chartWindowValue, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    chartBar->Add(m_chartWindowUnit, 0, wxALIGN_CENTER_VERTICAL);
    chartBar->AddStretchSpacer();
    chartBar->Add(chartHint, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);

    chartSizer->Add(chartBar, 0, wxEXPAND | wxALL, 8);
    chartSizer->Add(m_chartPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    chartPage->SetSizer(chartSizer);

    m_modeNotebook->AddPage(m_manualPage, _("Manual Mode"));
    m_modeNotebook->AddPage(m_interactivePage, _("Interactive Mode"));
    m_modeNotebook->AddPage(m_autoPage, _("Auto Mode"));
    m_modeNotebook->AddPage(chartPage, _("Chart"));
    m_modeNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SessionPanel::OnNotebookPageChanged, this);

    auto* manualSizer = new wxBoxSizer(wxVERTICAL);

    auto* opCard = new ui::CardPanel(m_manualPage);
    auto* opSizer = new wxBoxSizer(wxVERTICAL);
    auto* opTitle = new wxStaticText(opCard, wxID_ANY, _("Operation Request"));
    wxFont otf = opTitle->GetFont();
    otf.MakeBold();
    opTitle->SetFont(otf);
    opTitle->SetForegroundColour(kSubTextColor);
    opSizer->Add(opTitle, 0, wxALL, 12);

    m_varOpSizer = new wxBoxSizer(wxVERTICAL);
    auto* hint =
        new wxStaticText(opCard, wxID_ANY, _("Please select a variable from the left list."));
    hint->SetForegroundColour(kTextColor);
    m_varOpSizer->Add(hint, 0, wxALL, 10);
    opSizer->Add(m_varOpSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    opCard->SetSizer(opSizer);

    manualSizer->Add(opCard, 1, wxEXPAND | wxALL, 6);

    if (m_streamSession || m_mqttSession)
        BuildStreamUi(manualSizer, m_manualPage);
    else if (m_txSession)
        BuildTransactionUi(manualSizer, m_manualPage);

    m_manualPage->SetSizer(manualSizer);
    BuildTestFlowUi(m_interactivePage, false);
    BuildTestFlowUi(m_autoPage, true);

    auto* logPanel = new wxPanel(m_rightSplitter);
    logPanel->SetBackgroundColour(kBgColor);
    auto* logSizer = new wxBoxSizer(wxVERTICAL);

    auto* logCard = new ui::CardPanel(logPanel);
    auto* logMainSizer = new wxBoxSizer(wxVERTICAL);

    auto* logHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* titleLabel = new wxStaticText(logCard, wxID_ANY, _("Communication Log"));
    wxFont tf = titleLabel->GetFont();
    tf.SetPointSize(10);
    tf.MakeBold();
    titleLabel->SetFont(tf);
    titleLabel->SetForegroundColour(kSubTextColor);

    auto* clearBtn = new ui::FlatButton(logCard, _("Clear"), kBorderColor, kTextColor, [this]() {
        wxCommandEvent e;
        OnClear(e);
    });

    logHeaderSizer->Add(titleLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);
    logHeaderSizer->Add(clearBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    logMainSizer->Add(logHeaderSizer, 0, wxEXPAND | wxTOP | wxBOTTOM, 8);

    // 表头：时间 / 方向 / 内容，和 LogList 的绘制列保持一致
    auto* columnsPanel =
        new wxPanel(logCard, wxID_ANY, wxDefaultPosition, wxSize(-1, 28), wxBORDER_NONE);
    columnsPanel->SetBackgroundColour(wxColour(248, 250, 252));
    auto* columnsSizer = new wxBoxSizer(wxHORIZONTAL);
    auto addColumn = [&](const wxString& text, int width, int left) {
        auto* label = new wxStaticText(columnsPanel, wxID_ANY, text);
        label->SetFont(label->GetFont().Scaled(0.9).Bold());
        label->SetForegroundColour(wxColour(100, 116, 139));
        label->SetMinSize(wxSize(width, -1));
        columnsSizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, left);
        return label;
    };
    addColumn(_("Time"), ui::LogList::kTimeWidth, ui::LogList::kTimeX);
    addColumn(_("Direction"), ui::LogList::kDirectionWidth,
              ui::LogList::kDirectionX - ui::LogList::kTimeX - ui::LogList::kTimeWidth);
    auto* contentColumn = new wxStaticText(columnsPanel, wxID_ANY, _("Content"));
    contentColumn->SetFont(contentColumn->GetFont().Scaled(0.9).Bold());
    contentColumn->SetForegroundColour(wxColour(100, 116, 139));
    columnsSizer->Add(
        contentColumn, 1, wxALIGN_CENTER_VERTICAL | wxLEFT,
        ui::LogList::kContentX - ui::LogList::kDirectionX - ui::LogList::kDirectionWidth);
    columnsPanel->SetSizer(columnsSizer);
    logMainSizer->Add(columnsPanel, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

    m_commonLogCtrl = new ui::LogList(logCard);
    m_commonLogCtrl->SetShowHex(settings::UISettings::Instance().GetHexDisplay());
    m_commonLogCtrl->SetShowTime(settings::UISettings::Instance().GetShowTimestamp());
    m_commonLogCtrl->SetAutoScroll(settings::UISettings::Instance().GetAutoScroll());
    logMainSizer->Add(m_commonLogCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    logCard->SetSizer(logMainSizer);
    logSizer->Add(logCard, 1, wxEXPAND | wxALL, 6);
    logPanel->SetSizer(logSizer);

    m_rightSplitter->SplitHorizontally(m_modeNotebook, logPanel, 320);
    m_rightSplitter->SetMinimumPaneSize(150);

    m_mainSplitter->SplitVertically(varPanel, m_rightSplitter, 360);
    m_mainSplitter->SetMinimumPaneSize(180);

    main->Add(m_mainSplitter, 1, wxEXPAND | wxALL, 4);
    SetSizer(main);
}

wxBitmapButton* SessionPanel::CreateToolBtn(wxWindow* parent, int id, const wxString& iconName,
                                            const wxString& tooltip) {
    // ★ 原生 wxBitmapButton：命中测试、键盘焦点、禁用态全部交给系统处理，
    //   不再像之前那样用自绘 Panel 伪造按钮。
    auto* btn = new wxBitmapButton(parent, id, ui::LoadSvgIcon(iconName, 24, kIconTint),
                                   wxDefaultPosition, wxSize(36, 36), wxBORDER_NONE);
    btn->SetBackgroundColour(kCardBgColor);
    btn->SetToolTip(tooltip);

    ToolBtnState state;
    state.btn = btn;
    state.iconKey = iconName;
    state.enabled = true;
    state.tooltip = tooltip;
    m_toolBtnMap[id] = state;

    // ★ 直接处理点击：wxEVT_BUTTON -> OnToolClick，彻底不依赖 wxEVT_UPDATE_UI
    btn->Bind(wxEVT_BUTTON, &SessionPanel::OnToolClick, this);
    return btn;
}

void SessionPanel::SetToolBtn(int id, const wxString& iconName, bool enabled,
                              const wxString& tooltip) {
    auto it = m_toolBtnMap.find(id);
    if (it == m_toolBtnMap.end() || !it->second.btn) return;
    ToolBtnState& st = it->second;

    if (st.tooltip != tooltip) {
        st.btn->SetToolTip(tooltip);
        st.tooltip = tooltip;
    }
    if (st.enabled != enabled) {
        st.btn->Enable(enabled);
        st.enabled = enabled;
    }

    // 图标只在其名称真正变化时才重新渲染（解码 SVG + 着色是有成本的）。
    // 可用状态交给原生按钮自己处理：禁用时系统会自动把位图绘制成灰色。
    if (st.iconKey != iconName) {
        st.btn->SetBitmap(ui::LoadSvgIcon(iconName, 24, kIconTint));
        st.iconKey = iconName;
    }
}

void SessionPanel::UpdateToolbarState() {
    if (m_toolBtnMap.empty()) return;
    // 变量列表重建过程中会反复触发选中/取消选中，此时不必刷新工具栏
    if (m_refreshingVarList) return;

    const bool hasSession = (m_session != nullptr);
    const bool isVarCtx = (m_activeContext == ActiveContext::Variables);
    const bool isStepCtx = (m_activeContext == ActiveContext::Steps);
    const bool running = (m_testRunner != nullptr);
    const bool hasSelection = HasSelection();
    const SessionState state = m_session ? m_session->State() : SessionState::Idle;
    const bool canConfigure =
        hasSession && (state == SessionState::Idle || state == SessionState::Disconnected);

    // 连接 / 断开
    wxString actionIcon = "open";
    wxString actionTip = _("Open Connection (Ctrl+O)");
    if (hasSession && state == SessionState::Connected) {
        actionIcon = "disconnect";
        actionTip = (m_streamSession && m_streamSession->IsServerMode()) ? _("Stop Listening")
                                                                         : _("Disconnect");
    } else if (hasSession &&
               (state == SessionState::Connecting || state == SessionState::Reconnecting)) {
        actionIcon = "reconnecting";
        actionTip = _("Cancel");
    }
    SetToolBtn(idTbAction, actionIcon, hasSession, actionTip);

    SetToolBtn(idTbConfigure, "configure", canConfigure, _("Configure Connection"));

    // 录制中允许从任意页签停止，否则切到步骤页后就再也点不到“停止录制”了
    bool hasRecordableVar = false;
    if (m_sessionConfig) {
        for (const auto& [name, var] : m_sessionConfig->variables) {
            (void)name;
            if (var.enabled) {
                hasRecordableVar = true;
                break;
            }
        }
    }
    SetToolBtn(idTbRecord, m_isRecording ? "stoprecording" : "record",
               hasSession && (m_isRecording || hasRecordableVar),
               m_isRecording ? _("Stop Recording") : _("Record Data"));

    SetToolBtn(idTbHistory, "history", hasSession, _("History"));

    SetToolBtn(idTbAdd, "add", hasSession,
               isVarCtx ? _("Add Variable (Ctrl+N)") : _("Add Step (Ctrl+N)"));
    SetToolBtn(idTbCopy, "copy", hasSelection, _("Copy (Ctrl+C)"));
    SetToolBtn(idTbEdit, "edit", hasSelection, _("Edit (Ctrl+E)"));
    SetToolBtn(idTbDelete, "delete", hasSelection, _("Delete (Del)"));

    SetToolBtn(idTbRunStop, running ? "stop" : "run", hasSession && (running || isStepCtx),
               running ? _("Stop Flow") : _("Run Flow"));

    SetToolBtn(idTbBatch, "batch", hasSession && isStepCtx && hasSelection,
               _("Batch Create Steps"));
    SetToolBtn(idTbUp, "up", hasSession && isStepCtx && hasSelection, _("Move Up"));
    SetToolBtn(idTbDown, "down", hasSession && isStepCtx && hasSelection, _("Move Down"));

    UpdateRuntimeButtons();
}

void SessionPanel::UpdateRuntimeButtons() {
    if (!m_rtPauseBtn) return;

    const bool running = (m_testRunner != nullptr);
    const bool paused = running && m_testRunner->IsPaused();
    const bool failure = (m_pendingFailure != nullptr);
    const int current = running ? m_testRunner->CurrentStepIndex() : -1;
    wxListCtrl* list = m_activeStepList ? m_activeStepList : m_autoStepList;
    const int total = list ? static_cast<int>(list->GetItemCount()) - 1 : 0;

    m_rtPauseBtn->SetBtnText(paused ? _("Resume") : _("Pause"));
    m_rtPauseBtn->SetBtnEnabled(running && !failure);
    m_rtRetryBtn->SetBtnEnabled(running && (failure || paused));
    m_rtPrevBtn->SetBtnEnabled(running && paused && !failure && current > 0);
    m_rtNextBtn->SetBtnEnabled(running && paused && !failure && current >= 0 &&
                               current + 1 < total);
    m_rtJumpBtn->SetBtnEnabled(running && paused && !failure && total > 0);
    m_rtStopBtn->SetBtnEnabled(running);

    if (m_rtPauseBtn->GetParent()) m_rtPauseBtn->GetParent()->Layout();
}

void SessionPanel::RuntimeTogglePause() {
    if (!m_testRunner || m_pendingFailure) return;
    if (m_testRunner->IsPaused())
        m_testRunner->Resume();
    else
        m_testRunner->Pause();
    UpdateRuntimeButtons();
}

void SessionPanel::RuntimeRetry() {
    if (m_pendingFailure) {
        ResolvePendingFailure(StepFailureAction::Retry);
    } else if (m_testRunner && m_testRunner->IsPaused()) {
        m_testRunner->JumpToStep(std::max(0, m_testRunner->CurrentStepIndex()));
    }
    UpdateRuntimeButtons();
}

void SessionPanel::RuntimePrevious() {
    if (!m_testRunner || !m_testRunner->IsPaused() || m_pendingFailure) return;
    const int current = m_testRunner->CurrentStepIndex();
    m_testRunner->JumpToStep(std::max(0, current - 1));
    UpdateRuntimeButtons();
}

void SessionPanel::RuntimeNext() {
    if (!m_testRunner || !m_testRunner->IsPaused() || m_pendingFailure) return;
    m_testRunner->JumpToStep(m_testRunner->CurrentStepIndex() + 1);
    UpdateRuntimeButtons();
}

void SessionPanel::RuntimeJump() {
    if (!m_testRunner || !m_testRunner->IsPaused() || m_pendingFailure) return;
    wxListCtrl* list = m_activeStepList;
    if (!list || list->GetItemCount() <= 1) return;

    wxArrayString names;
    for (long i = 0; i < list->GetItemCount() - 1; ++i) names.Add(list->GetItemText(i));
    int selected = std::max(0, m_testRunner->CurrentStepIndex());
    if (ui::ChoiceDialog::Show(this, _("Jump to Step"), _("Select a step:"), names, selected,
                               selected)) {
        m_testRunner->JumpToStep(selected);
        UpdateRuntimeButtons();
    }
}

void SessionPanel::RuntimeStop() {
    if (m_pendingFailure) ResolvePendingFailure(StepFailureAction::Stop);
    if (m_testRunner) m_testRunner->Stop();
    UpdateRuntimeButtons();
}

void SessionPanel::BuildCommonUi(wxBoxSizer* mainSizer) {
    auto* topPanel = new wxPanel(this);
    topPanel->SetBackgroundColour(kCardBgColor);
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    m_titleLabel = new wxStaticText(topPanel, wxID_ANY, "-");
    wxFont f = m_titleLabel->GetFont();
    f.SetPointSize(f.GetPointSize() + 4);
    f.MakeBold();
    m_titleLabel->SetFont(f);
    m_titleLabel->SetForegroundColour(kThemeColor);
    topSizer->Add(m_titleLabel, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    auto* tbWrapper = new wxPanel(topPanel);
    tbWrapper->SetBackgroundColour(kCardBgColor);
    auto* tbSizer = new wxBoxSizer(wxHORIZONTAL);

    auto addBtn = [&](int id, const wxString& icon, const wxString& tooltip) {
        auto* btn = CreateToolBtn(tbWrapper, id, icon, tooltip);
        tbSizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    };

    auto addSep = [&]() {
        auto* sep = new wxPanel(tbWrapper, wxID_ANY, wxDefaultPosition, wxSize(1, 20));
        sep->SetBackgroundColour(kBorderColor);
        tbSizer->Add(sep, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
    };

    addBtn(idTbAction, "open", _("Open Connection (Ctrl+O)"));
    addBtn(idTbConfigure, "configure", _("Configure Connection"));
    addBtn(idTbRecord, "record", _("Record Data"));
    addBtn(idTbHistory, "history", _("History"));

    addSep();

    addBtn(idTbAdd, "add", _("Add (Ctrl+N)"));
    addBtn(idTbCopy, "copy", _("Copy (Ctrl+C)"));
    addBtn(idTbEdit, "edit", _("Edit (Ctrl+E)"));
    addBtn(idTbDelete, "delete", _("Delete (Del)"));

    addSep();

    addBtn(idTbRunStop, "run", _("Run Flow"));
    addBtn(idTbBatch, "batch", _("Batch Create Steps"));

    addSep();

    addBtn(idTbUp, "up", _("Move Up"));
    addBtn(idTbDown, "down", _("Move Down"));

    tbSizer->AddStretchSpacer();

    tbWrapper->SetSizer(tbSizer);
    topSizer->Add(tbWrapper, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);

    // ★ 注意：这里不再绑定 wxEVT_UPDATE_UI。按钮的可用状态 / 图标由
    //   UpdateToolbarState() 在会话状态、列表选中、模式切换等时机显式刷新。

    auto* infoRow = new wxBoxSizer(wxHORIZONTAL);

    m_statusLabel = new wxStaticText(topPanel, wxID_ANY, "-");
    wxFont f2 = m_statusLabel->GetFont();
    f2.MakeBold();
    f2.SetPointSize(10);
    m_statusLabel->SetFont(f2);
    infoRow->Add(m_statusLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);

    m_targetLabel = new wxStaticText(topPanel, wxID_ANY, "-", wxDefaultPosition, wxDefaultSize,
                                     wxST_ELLIPSIZE_END);
    m_targetLabel->SetForegroundColour(kSubTextColor);
    infoRow->Add(m_targetLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);

    m_rxLabel = new wxStaticText(topPanel, wxID_ANY, _("Received: 0 bytes"));
    m_txLabel = new wxStaticText(topPanel, wxID_ANY, _("Sent: 0 bytes"));
    m_clientsLabel = new wxStaticText(topPanel, wxID_ANY, _("Clients: 0"));
    m_rxLabel->SetForegroundColour(kSubTextColor);
    m_txLabel->SetForegroundColour(kSubTextColor);
    m_clientsLabel->SetForegroundColour(kSubTextColor);

    // 收发统计单独放一行右对齐，避免和目标地址挤在一起导致文字被裁剪/叠住
    auto* statsRow = new wxBoxSizer(wxHORIZONTAL);
    statsRow->AddStretchSpacer();
    statsRow->Add(m_rxLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    statsRow->Add(m_txLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    statsRow->Add(m_clientsLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    topSizer->Add(infoRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    topSizer->Add(statsRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* runtimeSeparator = new wxPanel(topPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    runtimeSeparator->SetBackgroundColour(kBorderColor);
    topSizer->Add(runtimeSeparator, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

    auto* runtimeSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* runtimeTitle = new wxStaticText(topPanel, wxID_ANY, _("Run Controls") + ":");
    runtimeTitle->SetForegroundColour(kSubTextColor);
    runtimeSizer->Add(runtimeTitle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    auto addRuntimeBtn = [&](const wxString& text, std::function<void()> onClick) {
        auto* btn =
            new ui::FlatButton(topPanel, text, ui::kBtnGrayBg, kTextColor, std::move(onClick));
        runtimeSizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        return btn;
    };
    m_rtPauseBtn = addRuntimeBtn(_("Pause"), [this]() { RuntimeTogglePause(); });
    m_rtRetryBtn = addRuntimeBtn(_("Retry"), [this]() { RuntimeRetry(); });
    m_rtPrevBtn = addRuntimeBtn(_("Previous Step"), [this]() { RuntimePrevious(); });
    m_rtNextBtn = addRuntimeBtn(_("Next Step"), [this]() { RuntimeNext(); });
    m_rtJumpBtn = addRuntimeBtn(_("Jump to Step..."), [this]() { RuntimeJump(); });
    m_rtStopBtn = addRuntimeBtn(_("Stop"), [this]() { RuntimeStop(); });
    runtimeSizer->AddStretchSpacer();

    topSizer->Add(runtimeSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);

    topPanel->SetSizer(topSizer);
    mainSizer->Add(topPanel, 0, wxEXPAND | wxALL, 6);
}

void SessionPanel::OnToolClick(wxCommandEvent& evt) {
    int id = evt.GetId();
    if (id == idTbAction)
        OnTbAction(evt);
    else if (id == idTbConfigure)
        OnTbConfigure(evt);
    else if (id == idTbRecord)
        OnTbRecord(evt);
    else if (id == idTbHistory)
        OnTbHistory(evt);
    else if (id == idTbAdd)
        OnTbAdd(evt);
    else if (id == idTbCopy)
        OnTbCopy(evt);
    else if (id == idTbEdit)
        OnTbEdit(evt);
    else if (id == idTbDelete)
        OnTbDelete(evt);
    else if (id == idTbRunStop)
        OnTbRunStop(evt);
    else if (id == idTbBatch)
        OnTbBatch(evt);
    else if (id == idTbUp)
        OnTbUp(evt);
    else if (id == idTbDown)
        OnTbDown(evt);
}

void SessionPanel::BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    (void)mainSizer;
    (void)parentWindow;
    if (m_streamSession && m_streamSession->IsServerMode())
        m_clientsLabel->Show();
    else
        m_clientsLabel->Hide();
}

void SessionPanel::BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    m_rxLabel->Hide();
    m_txLabel->Hide();
    m_clientsLabel->Hide();
}

wxWindow* SessionPanel::CreateJsonControl(wxWindow* parent, const FieldDescriptor& fd,
                                          const nlohmann::json& currentValue,
                                          std::function<void(const nlohmann::json&)> onChanged) {
    switch (fd.type) {
        case FieldValue::Type::String: {
            wxString defStr = currentValue.is_string()
                                  ? wxString::FromUTF8(currentValue.get<std::string>())
                                  : wxString::FromUTF8(fd.defaultValue.AsString());
            auto* t = new wxTextCtrl(parent, wxID_ANY, defStr, wxDefaultPosition, wxSize(-1, 28),
                                     wxBORDER_NONE);
            t->SetBackgroundColour(wxColour(249, 250, 251));
            t->Bind(wxEVT_TEXT, [t, onChanged](wxCommandEvent&) {
                onChanged(std::string(t->GetValue().utf8_str()));
            });
            return t;
        }
        case FieldValue::Type::Int: {
            int defInt =
                currentValue.is_number() ? currentValue.get<int>() : fd.defaultValue.AsInt();
            auto* s =
                new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 28),
                               wxSP_ARROW_KEYS | wxBORDER_NONE, fd.minValue, fd.maxValue, defInt);
            s->SetBackgroundColour(wxColour(249, 250, 251));
            s->Bind(wxEVT_SPINCTRL, [s, onChanged](wxSpinEvent&) { onChanged(s->GetValue()); });
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
            auto* ch = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 28), labels,
                                    wxBORDER_NONE);
            ch->SetBackgroundColour(wxColour(249, 250, 251));
            ch->SetSelection(sel);
            ch->Bind(wxEVT_CHOICE, [ch, fd, onChanged](wxCommandEvent&) {
                int idx = ch->GetSelection();
                if (idx >= 0 && idx < (int)fd.choices.size()) onChanged(fd.choices[idx].first);
            });
            return ch;
        }
    }
    return new wxTextCtrl(parent, wxID_ANY);
}

void SessionPanel::BuildVarOpUi(const CommVariable& var) {
    m_varOpSizer->Clear(true);
    m_varOpControls.clear();
    m_sendBtn = nullptr;
    m_readBtn = nullptr;
    m_writeBtn = nullptr;
    m_valueHintLabel = nullptr;

    auto* grid = new wxFlexGridSizer(2, 8, 8);
    grid->AddGrowableCol(1, 1);

    auto fields = m_protocol->VariableParamsFields();
    for (const auto& fd : fields) {
        auto* lbl = new wxStaticText(m_varOpSizer->GetContainingWindow(), wxID_ANY,
                                     wxGetTranslation(wxString::FromUTF8(fd.label)));
        lbl->SetForegroundColour(kTextColor);
        grid->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
        nlohmann::json curVal = var.params.contains(fd.key) ? var.params[fd.key] : nlohmann::json();
        wxWindow* ctrl = CreateJsonControl(
            m_varOpSizer->GetContainingWindow(), fd, curVal,
            [this, key = fd.key](const nlohmann::json& newVal) {
                if (!m_selectedVarName.empty() && m_sessionConfig)
                    m_sessionConfig->variables[m_selectedVarName].params[key] = newVal;
            });
        grid->Add(ctrl, 1, wxEXPAND);
        m_varOpControls[fd.key] = ctrl;
    }

    auto* lblVal =
        new wxStaticText(m_varOpSizer->GetContainingWindow(), wxID_ANY,
                         wxString::Format(_("Value (%s):"), wxString::FromUTF8(var.type)));
    lblVal->SetForegroundColour(kTextColor);
    grid->Add(lblVal, 0, wxALIGN_CENTER_VERTICAL);
    auto* valueCtrl = new wxTextCtrl(m_varOpSizer->GetContainingWindow(), wxID_ANY,
                                     wxString::FromUTF8(var.currentValue), wxDefaultPosition,
                                     wxSize(-1, 28), wxBORDER_NONE);
    valueCtrl->SetBackgroundColour(wxColour(249, 250, 251));
    valueCtrl->Bind(wxEVT_TEXT, [this, valueCtrl](wxCommandEvent&) {
        if (!m_selectedVarName.empty() && m_sessionConfig &&
            m_sessionConfig->variables.count(m_selectedVarName)) {
            auto& current = m_sessionConfig->variables[m_selectedVarName];
            const std::string text = std::string(valueCtrl->GetValue().utf8_str());
            current.currentValue = text;

            wxString rawStr;
            std::vector<uint8_t> bytes;
            std::string encodeError;
            if ((m_streamSession || m_mqttSession) &&
                ValueValidator::Encode(current.type, text, bytes, encodeError)) {
                for (uint8_t b : bytes) rawStr += wxString::Format("%02X ", b);
                rawStr = rawStr.Trim();
            } else if (m_txSession) {
                try {
                    const int val = std::stoi(text);
                    if (val >= 0 && val <= 65535) {
                        rawStr = wxString::Format("%02X %02X", (val >> 8) & 0xFF, val & 0xFF);
                    }
                } catch (...) {
                }
            }
            current.rawBytes = std::string(rawStr.utf8_str());
            if (m_varOpControls.count("__raw_bytes")) {
                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                    txt->ChangeValue(rawStr);
            }
            // 同步左侧“值”列，避免输入后列表仍显示旧值
            if (m_varList) {
                for (long i = 0; i < m_varList->GetItemCount() - 1; ++i) {
                    if (std::string(m_varList->GetItemText(i).utf8_str()) == m_selectedVarName) {
                        m_varList->SetItem(i, 2, valueCtrl->GetValue());
                        break;
                    }
                }
            }
            ValidateValueInput();
        }
    });
    if (var.access == "RO") {
        valueCtrl->SetEditable(false);
        valueCtrl->SetBackgroundColour(wxColour(240, 240, 240));
    }
    grid->Add(valueCtrl, 1, wxEXPAND);
    m_varOpControls["__value"] = valueCtrl;

    auto* lblRaw = new wxStaticText(m_varOpSizer->GetContainingWindow(), wxID_ANY, _("Raw Bytes:"));
    lblRaw->SetForegroundColour(kTextColor);
    grid->Add(lblRaw, 0, wxALIGN_CENTER_VERTICAL);
    auto* rawCtrl = new wxTextCtrl(m_varOpSizer->GetContainingWindow(), wxID_ANY,
                                   wxString::FromUTF8(var.rawBytes), wxDefaultPosition,
                                   wxSize(-1, 28), wxTE_READONLY | wxBORDER_NONE);
    rawCtrl->SetBackgroundColour(wxColour(249, 250, 251));
    grid->Add(rawCtrl, 1, wxEXPAND);
    m_varOpControls["__raw_bytes"] = rawCtrl;

    m_varOpSizer->Add(grid, 0, wxEXPAND | wxALL, 8);

    // 校验提示行：紧贴输入区，非法类型时显示红色错误
    m_valueHintLabel = new wxStaticText(m_varOpSizer->GetContainingWindow(), wxID_ANY, "");
    m_valueHintLabel->SetForegroundColour(wxColour(220, 38, 38));
    m_varOpSizer->Add(m_valueHintLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    m_varOpSizer->AddStretchSpacer(1);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    if (m_streamSession || m_mqttSession) {
        if (var.access != "RO") {
            m_sendBtn = new ui::FlatButton(m_varOpSizer->GetContainingWindow(), _("Send"),
                                           kThemeColor, *wxWHITE, [this]() {
                                               wxCommandEvent e;
                                               OnExecuteSend(e);
                                           });
            btnSizer->Add(m_sendBtn, 0, wxALL, 4);
        }
    } else if (m_txSession) {
        m_readBtn = new ui::FlatButton(m_varOpSizer->GetContainingWindow(), _("Read"), kThemeColor,
                                       *wxWHITE, [this]() {
                                           wxCommandEvent e;
                                           OnExecuteRead(e);
                                       });
        m_writeBtn = new ui::FlatButton(m_varOpSizer->GetContainingWindow(), _("Write"),
                                        kThemeColor, *wxWHITE, [this]() {
                                            wxCommandEvent e;
                                            OnExecuteWrite(e);
                                        });
        if (var.access == "RO") m_writeBtn->SetBtnEnabled(false);
        if (var.access == "WO") m_readBtn->SetBtnEnabled(false);
        btnSizer->Add(m_readBtn, 0, wxALL, 4);
        btnSizer->Add(m_writeBtn, 0, wxALL, 4);
    }

    m_varOpSizer->Add(btnSizer, 0, wxALIGN_RIGHT);
    ValidateValueInput();
    m_varOpSizer->Layout();
    m_manualPage->Layout();
}

void SessionPanel::ValidateValueInput() {
    if (!m_valueHintLabel) return;
    if (m_selectedVarName.empty() || !m_sessionConfig ||
        !m_sessionConfig->variables.count(m_selectedVarName)) {
        return;
    }

    const auto& var = m_sessionConfig->variables[m_selectedVarName];
    wxTextCtrl* valueCtrl = nullptr;
    if (m_varOpControls.count("__value"))
        valueCtrl = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]);
    if (!valueCtrl) return;

    const std::string text = std::string(valueCtrl->GetValue().utf8_str());
    std::string error;

    bool ok = ValueValidator::Validate(var.type, text, error);
    if (ok && m_txSession) {
        // Modbus 寄存器是 16 位无符号；这里额外限制写入范围
        try {
            const long long v = std::stoll(ValueValidator::Trim(text));
            if (v < 0 || v > 65535) {
                ok = false;
                error = std::string(_("Modbus register value must be 0..65535.").utf8_str());
            }
        } catch (...) {
            ok = false;
            error = std::string(_("Modbus register value must be an integer.").utf8_str());
        }
    }

    const bool empty = text.empty();
    if (empty) {
        m_valueHintLabel->SetForegroundColour(kSubTextColor);
        m_valueHintLabel->SetLabel(_("Please enter a value."));
        m_valueHintLabel->Show();
    } else if (!ok) {
        m_valueHintLabel->SetForegroundColour(wxColour(220, 38, 38));
        m_valueHintLabel->SetLabel(_("Invalid value: ") + wxString::FromUTF8(error));
        m_valueHintLabel->Show();
    } else {
        m_valueHintLabel->SetLabel("");
        m_valueHintLabel->Hide();
    }

    const bool hasValue = !empty;
    if (m_sendBtn) m_sendBtn->SetBtnEnabled(ok && hasValue && var.access != "RO");
    if (m_writeBtn) m_writeBtn->SetBtnEnabled(ok && hasValue && var.access != "RO");
    m_varOpSizer->Layout();
}

void SessionPanel::RefreshVariableList() {
    if (!m_varList || !m_sessionConfig) return;

    // 按用户保存的顺序显示；缺失的变量按名称追加
    std::vector<std::string> ordered;
    ordered.reserve(m_sessionConfig->variables.size());
    for (const auto& name : m_sessionConfig->variableOrder) {
        if (m_sessionConfig->variables.count(name)) ordered.push_back(name);
    }
    for (const auto& [name, var] : m_sessionConfig->variables) {
        if (std::find(ordered.begin(), ordered.end(), name) == ordered.end())
            ordered.push_back(name);
    }
    m_sessionConfig->variableOrder = ordered;

    // 重建期间抑制选中事件：否则每收到一帧数据都会重建右侧操作区并抢走焦点。
    m_refreshingVarList = true;
    m_varList->DeleteAllItems();
    int idx = 0;
    long restoreRow = -1;
    for (const auto& name : ordered) {
        const auto& var = m_sessionConfig->variables.at(name);
        long item = m_varList->InsertItem(idx++, wxString::FromUTF8(name));
        m_varList->SetItem(item, 1, wxString::FromUTF8(var.type));
        m_varList->SetItem(item, 2, wxString::FromUTF8(var.currentValue));
        if (!m_selectedVarName.empty() && name == m_selectedVarName) restoreRow = item;
    }
    long newItem = m_varList->InsertItem(idx, _("+ New Variable"));
    m_varList->SetItemTextColour(newItem, kThemeColor);

    // 保留原有选中行，避免刷新后 Edit/Delete 等按钮突然全部变灰
    if (restoreRow != -1) {
        m_varList->SetItemState(restoreRow, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        m_varList->EnsureVisible(restoreRow);
    }
    m_refreshingVarList = false;

    UpdateToolbarState();
}

void SessionPanel::OnVarSelected(wxListEvent& evt) {
    long item = evt.GetIndex();
    if (item < 0) return;
    if (m_refreshingVarList) return;
    if (item == m_varList->GetItemCount() - 1) {
        m_varList->SetItemState(item, 0, wxLIST_STATE_SELECTED);
        return;
    }
    m_activeContext = ActiveContext::Variables;
    m_selectedVarName = std::string(m_varList->GetItemText(item).utf8_str());
    if (m_sessionConfig && m_sessionConfig->variables.count(m_selectedVarName)) {
        BuildVarOpUi(m_sessionConfig->variables[m_selectedVarName]);
    }
    UpdateToolbarState();
}

void SessionPanel::OnExecuteRead(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    auto varCopy = m_sessionConfig->variables[m_selectedVarName];

    TransactionRequest req;
    req.slaveId = varCopy.params.value("slaveId", 1);
    if (varCopy.params.contains("functionCode")) {
        if (varCopy.params["functionCode"].is_string())
            req.functionCode = std::stoi(varCopy.params["functionCode"].get<std::string>());
        else
            req.functionCode = varCopy.params["functionCode"].get<int>();
    }
    req.startAddress = varCopy.params.value("startAddress", 0);
    req.count = varCopy.params.value("count", 1);

    std::thread([this, req, name = m_selectedVarName]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name]() {
            if (resp.ok && !resp.registers.empty()) {
                std::string valStr = std::to_string(resp.registers[0]);
                m_sessionConfig->variables[name].currentValue = valStr;
                std::vector<uint8_t> raw;
                for (auto r : resp.registers) {
                    raw.push_back((r >> 8) & 0xFF);
                    raw.push_back(r & 0xFF);
                }
                wxString rawStr;
                for (uint8_t b : raw) rawStr += wxString::Format("%02X ", b);
                m_sessionConfig->variables[name].rawBytes = std::string(rawStr.Trim().utf8_str());
                if (m_varOpControls.count("__value"))
                    if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                        txt->ChangeValue(valStr);
                if (m_varOpControls.count("__raw_bytes"))
                    if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                        txt->ChangeValue(rawStr);
                RefreshVariableList();
                AppendUnifiedLog(true, name, m_sessionConfig->variables[name].type, valStr, raw);
                RecordVariableValue(name, valStr);
            } else {
                AppendUnifiedLog(true, name, "Error", resp.errorMessage, {});
            }
        });
    }).detach();
}

void SessionPanel::OnExecuteWrite(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    auto varCopy = m_sessionConfig->variables[m_selectedVarName];

    auto* valueCtrl = m_varOpControls.count("__value")
                          ? dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])
                          : nullptr;
    if (!valueCtrl) return;
    const std::string targetVal = std::string(valueCtrl->GetValue().utf8_str());

    std::string error;
    bool ok = ValueValidator::Validate(varCopy.type, targetVal, error);
    if (ok) {
        try {
            const long long v = std::stoll(ValueValidator::Trim(targetVal));
            if (v < 0 || v > 65535) {
                ok = false;
                error = std::string(_("Modbus register value must be 0..65535.").utf8_str());
            }
        } catch (...) {
            ok = false;
            error = std::string(_("Modbus register value must be an integer.").utf8_str());
        }
    }
    if (!ok) {
        ValidateValueInput();
        ShowMsg(wxString::Format(_("Write failed: invalid value for type %s.\n\n%s"),
                                 wxString::FromUTF8(varCopy.type), wxString::FromUTF8(error)),
                _("Invalid Value"), wxOK | wxICON_WARNING, this);
        return;
    }

    TransactionRequest req;
    req.slaveId = varCopy.params.value("slaveId", 1);
    int fc = 6;
    if (varCopy.params.contains("functionCode")) {
        if (varCopy.params["functionCode"].is_string())
            fc = std::stoi(varCopy.params["functionCode"].get<std::string>());
        else
            fc = varCopy.params["functionCode"].get<int>();
    }
    if (fc == 1)
        fc = 5;
    else if (fc == 3)
        fc = 6;
    else if (fc == 4)
        fc = 6;
    req.functionCode = fc;
    req.startAddress = varCopy.params.value("startAddress", 0);
    req.writeValues.push_back(static_cast<uint16_t>(std::stoi(targetVal)));

    std::thread([this, req, name = m_selectedVarName, targetVal]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name, targetVal]() {
            if (resp.ok) {
                m_sessionConfig->variables[name].currentValue = targetVal;
                std::vector<uint8_t> raw;
                try {
                    int val = std::stoi(targetVal);
                    raw.push_back((val >> 8) & 0xFF);
                    raw.push_back(val & 0xFF);
                } catch (...) {
                }
                wxString rawStr;
                for (uint8_t b : raw) rawStr += wxString::Format("%02X ", b);
                m_sessionConfig->variables[name].rawBytes = std::string(rawStr.Trim().utf8_str());
                if (m_varOpControls.count("__raw_bytes"))
                    if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                        txt->ChangeValue(rawStr);
                RefreshVariableList();
                AppendUnifiedLog(false, name, m_sessionConfig->variables[name].type, targetVal,
                                 raw);
                RecordVariableValue(name, targetVal);
            } else {
                AppendUnifiedLog(false, name, "Error", resp.errorMessage, {});
            }
        });
    }).detach();
}

bool SessionPanel::ValidateSelectedValue(const wxString& action) {
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return false;

    const auto& var = m_sessionConfig->variables[m_selectedVarName];
    const std::string text = var.currentValue;
    if (text.empty()) {
        ShowMsg(_("Payload is empty. Please fill in the 'Value' field."), _("Notice"),
                wxOK | wxICON_WARNING, this);
        return false;
    }

    std::string error;
    bool ok = ValueValidator::Validate(var.type, text, error);
    if (ok && m_txSession) {
        try {
            const long long v = std::stoll(ValueValidator::Trim(text));
            if (v < 0 || v > 65535) {
                ok = false;
                error = std::string(_("Modbus register value must be 0..65535.").utf8_str());
            }
        } catch (...) {
            ok = false;
            error = std::string(_("Modbus register value must be an integer.").utf8_str());
        }
    }

    if (!ok) {
        ValidateValueInput();
        ShowMsg(wxString::Format(_("%s failed: invalid value for type %s.\n\n%s"), action,
                                 wxString::FromUTF8(var.type), wxString::FromUTF8(error)),
                _("Invalid Value"), wxOK | wxICON_WARNING, this);
        return false;
    }
    return true;
}

void SessionPanel::OnExecuteSend(wxCommandEvent&) {
    if (m_mqttSession && m_mqttSession->State() == SessionState::Connected) {
        if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
        if (!ValidateSelectedValue(_("Send"))) return;

        auto& var = m_sessionConfig->variables[m_selectedVarName];
        std::string payload = var.currentValue;
        std::string topic = var.params.value("topic", "test/topic");
        int qos = var.params.value("qos", 0);
        m_mqttSession->Publish(topic, payload, qos);
        std::vector<uint8_t> bytes(payload.begin(), payload.end());
        m_txBytes += bytes.size();
        if (m_txLabel)
            m_txLabel->SetLabel(
                wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));
        wxString rawStr;
        for (uint8_t b : bytes) rawStr += wxString::Format("%02X ", b);
        var.rawBytes = std::string(rawStr.Trim().utf8_str());
        if (m_varOpControls.count("__raw_bytes"))
            if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                txt->ChangeValue(rawStr.Trim());
        AppendUnifiedLog(false, m_selectedVarName, var.type, payload, bytes);
        RecordVariableValue(m_selectedVarName, payload);
        return;
    }

    if ((!m_streamSession || !m_streamSession->IsConnected())) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    if (!ValidateSelectedValue(_("Send"))) return;

    auto& var = m_sessionConfig->variables[m_selectedVarName];
    const std::string payload = var.currentValue;

    std::vector<uint8_t> bytes;
    std::string encodeError;
    if (!ValueValidator::Encode(var.type, payload, bytes, encodeError)) {
        ShowMsg(wxString::Format(_("Send failed: invalid value for type %s.\n\n%s"),
                                 wxString::FromUTF8(var.type), wxString::FromUTF8(encodeError)),
                _("Invalid Value"), wxOK | wxICON_WARNING, this);
        return;
    }

    if (m_streamSession->Send(bytes)) {
        m_txBytes += bytes.size();
        if (m_txLabel)
            m_txLabel->SetLabel(
                wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));
        wxString rawStr;
        for (uint8_t b : bytes) rawStr += wxString::Format("%02X ", b);
        var.rawBytes = std::string(rawStr.Trim().utf8_str());
        if (m_varOpControls.count("__raw_bytes"))
            if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                txt->ChangeValue(rawStr.Trim());
        AppendUnifiedLog(false, m_selectedVarName, var.type, payload, bytes);
        RecordVariableValue(m_selectedVarName, payload);
    } else {
        ShowMsg(_("Send failed"), _("Error"), wxOK | wxICON_ERROR);
    }
}

void SessionPanel::DoAddVariable() {
    if (!m_sessionConfig) return;
    if (m_varList && m_varList->GetItemCount() > 0) {
        m_varList->SetItemState(m_varList->GetItemCount() - 1, 0, wxLIST_STATE_SELECTED);
    }
    std::string baseName = "new_var";
    std::string name = baseName;
    int suffix = 1;
    while (m_sessionConfig->variables.count(name)) name = baseName + "_" + std::to_string(suffix++);

    nlohmann::json defaultJson = {
        {"type", "int32"}, {"access", "RW"}, {"params", nlohmann::json::object()}};
    VariableEditDialog dlg(this, m_protocol, name, defaultJson, false);
    if (dlg.ShowModal() == wxID_OK) {
        std::string newName = dlg.GetVarName();
        if (newName.empty()) return;
        auto j = dlg.GetVarJson();
        CommVariable var;
        var.name = newName;
        var.type = j.value("type", "int32");
        var.access = j.value("access", "RW");
        if (j.contains("params")) var.params = j["params"];
        var.enabled = true;
        m_sessionConfig->variables[newName] = var;
        m_sessionConfig->variableOrder.push_back(newName);
        RefreshVariableList();
        for (long i = 0; i < m_varList->GetItemCount() - 1; ++i) {
            if (std::string(m_varList->GetItemText(i).utf8_str()) == newName) {
                m_varList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                m_varList->EnsureVisible(i);
                break;
            }
        }
        UpdateToolbarState();
    }
}

void SessionPanel::DoCopyVariable() {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == m_varList->GetItemCount() - 1) return;
    std::string oldName = std::string(m_varList->GetItemText(sel).utf8_str());
    auto it = m_sessionConfig->variables.find(oldName);
    if (it == m_sessionConfig->variables.end()) return;
    std::string newName = oldName + "_copy";
    int suffix = 1;
    while (m_sessionConfig->variables.count(newName))
        newName = oldName + "_copy" + std::to_string(suffix++);
    CommVariable newVar = it->second;
    newVar.name = newName;
    m_sessionConfig->variables[newName] = newVar;

    auto& order = m_sessionConfig->variableOrder;
    auto pos = std::find(order.begin(), order.end(), oldName);
    if (pos != order.end())
        order.insert(pos + 1, newName);
    else
        order.push_back(newName);

    RefreshVariableList();
    UpdateToolbarState();
}

void SessionPanel::DoEditVariable() {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == m_varList->GetItemCount() - 1) return;

    std::string oldName = std::string(m_varList->GetItemText(sel).utf8_str());
    auto it = m_sessionConfig->variables.find(oldName);
    if (it == m_sessionConfig->variables.end()) return;

    nlohmann::json editJson = {
        {"type", it->second.type}, {"access", it->second.access}, {"params", it->second.params}};
    VariableEditDialog dlg(this, m_protocol, oldName, editJson, true);

    if (dlg.ShowModal() == wxID_OK) {
        std::string newName = dlg.GetVarName();
        if (newName.empty()) return;

        auto j = dlg.GetVarJson();

        if (newName != oldName) {
            if (m_sessionConfig->variables.count(newName)) {
                ShowMsg(_("Variable name already exists."), _("Error"), wxOK | wxICON_ERROR, this);
                return;
            }
            CommVariable var = it->second;
            var.name = newName;
            var.type = j.value("type", "int32");
            var.access = j.value("access", "RW");
            if (j.contains("params")) var.params = j["params"];

            m_sessionConfig->variables.erase(it);
            m_sessionConfig->variables[newName] = var;

            auto& order = m_sessionConfig->variableOrder;
            auto pos = std::find(order.begin(), order.end(), oldName);
            if (pos != order.end())
                *pos = newName;
            else
                order.push_back(newName);

            auto updateFlow = [&](nlohmann::json& flow) {
                if (!flow.is_array()) return;
                for (auto& step : flow) {
                    if (step.contains("variable") && step["variable"] == oldName) {
                        step["variable"] = newName;
                    }
                }
            };
            updateFlow(m_sessionConfig->autoMode.testFlow);
            updateFlow(m_sessionConfig->interactiveMode.testFlow);

            m_selectedVarName = newName;
            BuildVarOpUi(m_sessionConfig->variables[newName]);
        } else {
            it->second.type = j.value("type", "int32");
            it->second.access = j.value("access", "RW");
            if (j.contains("params")) it->second.params = j["params"];
        }

        RefreshVariableList();

        for (long i = 0; i < m_varList->GetItemCount() - 1; ++i) {
            if (std::string(m_varList->GetItemText(i).utf8_str()) == newName) {
                m_varList->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                m_varList->EnsureVisible(i);
                break;
            }
        }
        UpdateToolbarState();
    }
}

void SessionPanel::DoDeleteVariable() {
    if (!m_sessionConfig || !m_varList) return;
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == m_varList->GetItemCount() - 1) return;

    const std::string removedName = std::string(m_varList->GetItemText(sel).utf8_str());
    m_sessionConfig->variables.erase(removedName);
    auto& order = m_sessionConfig->variableOrder;
    order.erase(std::remove(order.begin(), order.end(), removedName), order.end());
    m_selectedVarName.clear();
    m_varOpControls.clear();
    m_varOpSizer->Clear(true);

    auto* hint = new wxStaticText(m_varOpSizer->GetContainingWindow(), wxID_ANY,
                                  _("Please select a variable from the left list."));
    hint->SetForegroundColour(kTextColor);
    m_varOpSizer->Add(hint, 0, wxALL, 10);
    m_varOpSizer->Layout();
    m_manualPage->Layout();

    RefreshVariableList();
    UpdateToolbarState();
}

void SessionPanel::OnNotebookPageChanged(wxBookCtrlEvent& evt) {
    int sel = m_modeNotebook ? m_modeNotebook->GetSelection() : -1;
    if (m_sessionConfig && sel >= 0 && sel <= 2) {
        m_sessionConfig->activeMode = static_cast<SessionMode>(sel);
        m_activeContext = (sel == 0) ? ActiveContext::Variables : ActiveContext::Steps;
    }
    UpdateToolbarState();
    evt.Skip();
}

void SessionPanel::RefreshStepList(wxListCtrl* stepList, const nlohmann::json& flow) {
    if (!stepList) return;
    stepList->DeleteAllItems();
    int idx = 0;
    if (flow.is_array()) {
        for (const auto& s : flow) {
            long item = stepList->InsertItem(idx++, wxString::FromUTF8(s.value("name", "Unnamed")));
            stepList->SetItem(item, 1, _("Pending"));
            stepList->SetItem(item, 2, DescribeStep(s));
            stepList->SetItemTextColour(item, kSubTextColor);
            stepList->SetItemBackgroundColour(item, kCardBgColor);
        }
    }
    long newItem = stepList->InsertItem(idx, _("+ New Step"));
    stepList->SetItemTextColour(newItem, kThemeColor);
    UpdateStepProgress(stepList, 0, "");
}

void SessionPanel::OnListLeftDown(wxListCtrl* list, const wxPoint& pos) {
    if (!list) return;
    m_dragList = list;
    m_dragStart = pos;
    m_dragging = false;
    int flags = 0;
    const long item = list->HitTest(pos, flags, nullptr);
    m_dragIndex = (item != -1 && item != list->GetItemCount() - 1) ? item : -1;
}

void SessionPanel::OnListMotion(wxListCtrl* list, const wxPoint& pos) {
    if (!list || list != m_dragList || m_dragIndex < 0) return;
    if (m_dragging) return;
    if (!wxGetMouseState().LeftIsDown()) {
        CancelListDrag();
        return;
    }
    if (std::abs(pos.x - m_dragStart.x) < 5 && std::abs(pos.y - m_dragStart.y) < 5) return;

    m_dragging = true;
    if (!list->HasCapture()) list->CaptureMouse();
    list->SetCursor(wxCursor(wxCURSOR_SIZENS));
}

void SessionPanel::OnListLeftUp(wxListCtrl* list, const wxPoint& pos) {
    if (!list || list != m_dragList) {
        CancelListDrag();
        return;
    }

    if (m_dragging && m_dragIndex >= 0 && m_sessionConfig) {
        int flags = 0;
        long to = list->HitTest(pos, flags, nullptr);
        const long maxIndex = list->GetItemCount() - 2;
        if (to < 0) to = maxIndex;
        if (to > maxIndex) to = maxIndex;

        if (to >= 0 && to != m_dragIndex) {
            if (list == m_varList) {
                MoveVariableOrder(m_dragIndex, to);
            } else {
                auto& flow = (list == m_autoStepList) ? m_sessionConfig->autoMode.testFlow
                                                      : m_sessionConfig->interactiveMode.testFlow;
                MoveStepOrder(list, flow, m_dragIndex, to);
            }
        }
    }

    CancelListDrag();
}

void SessionPanel::CancelListDrag() {
    if (m_dragList) {
        if (m_dragList->HasCapture()) m_dragList->ReleaseMouse();
        m_dragList->SetCursor(wxCursor(wxCURSOR_ARROW));
    }
    m_dragList = nullptr;
    m_dragIndex = -1;
    m_dragging = false;
}

void SessionPanel::UpdateStepProgress(wxListCtrl* list, int completed, const wxString& text) {
    (void)text;
    if (!list) return;

    const bool autoMode = (list == m_autoStepList);
    ui::ProgressBar* bar = autoMode ? m_autoProgressBar : m_interactiveProgressBar;
    wxStaticText* ratioLabel = autoMode ? m_autoRatioLabel : m_interactiveRatioLabel;
    if (!bar) return;

    const int total = std::max(0, static_cast<int>(list->GetItemCount()) - 1);
    const int value = std::max(0, std::min(completed, total));
    bar->SetProgress(value, total);
    if (ratioLabel) ratioLabel->SetLabel(wxString::Format("%d / %d", value, total));
    UpdateRunTiming(list);
}

void SessionPanel::UpdateRunTiming(wxListCtrl* list) {
    const bool autoMode = (list == m_autoStepList);
    wxStaticText* label = autoMode ? m_autoTimeLabel : m_interactiveTimeLabel;
    if (!label) return;

    if (!m_runTimingActive) {
        label->SetLabel(
            wxString::Format(_("Est.: %s, Elapsed: %s, Remaining: %s"), "--", "--", "--"));
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const long long elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_runStart).count();
    const long long estimatedMs = m_plannedWaitMs + m_extraWaitMs;
    const long long remainingMs = std::max(0LL, estimatedMs - elapsedMs);
    label->SetLabel(wxString::Format(_("Est.: %s, Elapsed: %s, Remaining: %s"),
                                     FormatDurationCN(estimatedMs), FormatDurationCN(elapsedMs),
                                     FormatDurationCN(remainingMs)));
}

void SessionPanel::HighlightRunningStep(wxListCtrl* list, long row) {
    if (!list || row < 0 || row >= list->GetItemCount() - 1) return;
    if (m_runningStepList == list && m_runningStepRow == row) {
        // 重试同一步时把失败时的红色背景恢复成运行中的主题色
        list->SetItemBackgroundColour(row, ui::kSelectBgColor);
        list->SetItemState(row, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
        list->EnsureVisible(row);
        return;
    }

    ClearRunningHighlight();
    for (long i = 0; i < list->GetItemCount() - 1; ++i) {
        if (i != row) list->SetItemState(i, 0, wxLIST_STATE_SELECTED);
    }
    list->SetItemBackgroundColour(row, ui::kSelectBgColor);
    list->SetItemState(row, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                       wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    list->EnsureVisible(row);
    m_runningStepList = list;
    m_runningStepRow = row;
}

void SessionPanel::ClearRunningHighlight() {
    if (m_runningStepList && m_runningStepRow >= 0 &&
        m_runningStepRow < m_runningStepList->GetItemCount() - 1) {
        m_runningStepList->SetItemBackgroundColour(m_runningStepRow, kCardBgColor);
    }
    m_runningStepList = nullptr;
    m_runningStepRow = -1;
}

void SessionPanel::MoveVariableOrder(long from, long to) {
    if (!m_sessionConfig) return;
    auto& order = m_sessionConfig->variableOrder;
    if (from < 0 || to < 0 || from >= static_cast<long>(order.size()) ||
        to >= static_cast<long>(order.size())) {
        return;
    }
    std::string name = order[from];
    order.erase(order.begin() + from);
    order.insert(order.begin() + to, name);
    m_selectedVarName = name;
    RefreshVariableList();
}

void SessionPanel::MoveStepOrder(wxListCtrl* stepList, nlohmann::json& flow, long from, long to) {
    if (!stepList || !flow.is_array()) return;
    if (from < 0 || to < 0 || from >= static_cast<long>(flow.size()) ||
        to >= static_cast<long>(flow.size())) {
        return;
    }
    nlohmann::json step = flow[from];
    flow.erase(flow.begin() + from);
    flow.insert(flow.begin() + to, step);
    RefreshStepList(stepList, flow);
    stepList->SetItemState(to, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    stepList->EnsureVisible(to);
    UpdateToolbarState();
}

void SessionPanel::ShowVarContextMenu(const wxPoint& pos) {
    if (!m_sessionConfig || !m_varList) return;
    const wxPoint menuPos = ContextMenuPos(m_varList, pos);

    int flags = 0;
    const long item = m_varList->HitTest(menuPos, flags, nullptr);
    const bool onAddRow = (item != -1 && item == m_varList->GetItemCount() - 1);
    const bool hasSelection = (item != -1 && !onAddRow);

    if (hasSelection) {
        m_activeContext = ActiveContext::Variables;
        m_varList->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    enum { kAdd = 1, kCopy, kEdit, kDelete, kUp, kDown };
    wxMenu menu;
    menu.Append(kAdd, _("Add Variable"));
    menu.Append(kCopy, _("Copy"));
    menu.Append(kEdit, _("Edit"));
    menu.Append(kDelete, _("Delete"));
    menu.AppendSeparator();
    menu.Append(kUp, _("Move Up"));
    menu.Append(kDown, _("Move Down"));
    menu.Enable(kCopy, hasSelection);
    menu.Enable(kEdit, hasSelection);
    menu.Enable(kDelete, hasSelection);
    menu.Enable(kUp, hasSelection && item > 0);
    menu.Enable(kDown, hasSelection && item < m_varList->GetItemCount() - 2);

    const int cmd = m_varList->GetPopupMenuSelectionFromUser(menu, menuPos);
    switch (cmd) {
        case kAdd:
            DoAddVariable();
            break;
        case kCopy:
            DoCopyVariable();
            break;
        case kEdit:
            DoEditVariable();
            break;
        case kDelete:
            DoDeleteVariable();
            break;
        case kUp:
            if (hasSelection && item > 0) MoveVariableOrder(item, item - 1);
            break;
        case kDown:
            if (hasSelection && item < m_varList->GetItemCount() - 2)
                MoveVariableOrder(item, item + 1);
            break;
        default:
            break;
    }
}

void SessionPanel::ShowStepContextMenu(wxListCtrl* stepList, const wxPoint& pos) {
    if (!m_sessionConfig || !stepList) return;
    const wxPoint menuPos = ContextMenuPos(stepList, pos);

    int flags = 0;
    const long item = stepList->HitTest(menuPos, flags, nullptr);
    const bool onAddRow = (item != -1 && item == stepList->GetItemCount() - 1);
    const bool hasSelection = (item != -1 && !onAddRow);

    if (hasSelection) {
        m_activeContext = ActiveContext::Steps;
        stepList->SetItemState(item, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    enum {
        kPause = 1,
        kRetry,
        kStop,
        kPrev,
        kNext,
        kJump,
        kAdd = 20,
        kCopy,
        kEdit,
        kDelete,
        kBatch,
        kUp,
        kDown
    };
    wxMenu menu;

    if (m_testRunner) {
        const int current = m_testRunner->CurrentStepIndex();
        const int total = static_cast<int>(stepList->GetItemCount()) - 1;
        const bool paused = m_testRunner->IsPaused();
        const bool failure = (m_pendingFailure != nullptr);

        menu.Append(kPause, paused ? _("Resume") : _("Pause"));
        menu.Append(kRetry, _("Retry"));
        menu.Append(kPrev, _("Previous Step"));
        menu.Append(kNext, _("Next Step"));
        menu.Append(kJump, _("Jump to Step..."));
        menu.Append(kStop, _("Stop"));
        menu.Enable(kPause, !failure);
        menu.Enable(kRetry, failure || paused);
        menu.Enable(kPrev, !failure && paused && current > 0);
        menu.Enable(kNext, !failure && paused && current >= 0 && current + 1 < total);
        menu.Enable(kJump, !failure && paused && total > 0);
        menu.Enable(kStop, true);
        menu.AppendSeparator();
    }

    menu.Append(kAdd, _("Add Step"));
    menu.Append(kCopy, _("Copy"));
    menu.Append(kEdit, _("Edit"));
    menu.Append(kDelete, _("Delete"));
    menu.AppendSeparator();
    menu.Append(kBatch, _("Batch Create Steps"));
    menu.AppendSeparator();
    menu.Append(kUp, _("Move Up"));
    menu.Append(kDown, _("Move Down"));
    menu.Enable(kCopy, hasSelection);
    menu.Enable(kEdit, hasSelection);
    menu.Enable(kDelete, hasSelection);
    menu.Enable(kBatch, hasSelection);
    menu.Enable(kUp, hasSelection && item > 0);
    menu.Enable(kDown, hasSelection && item < stepList->GetItemCount() - 2);

    const int cmd = stepList->GetPopupMenuSelectionFromUser(menu, menuPos);
    switch (cmd) {
        case kPause:
            if (m_testRunner) {
                if (m_testRunner->IsPaused())
                    m_testRunner->Resume();
                else
                    m_testRunner->Pause();
            }
            break;
        case kRetry:
            if (m_pendingFailure) {
                ResolvePendingFailure(StepFailureAction::Retry);
            } else if (m_testRunner) {
                const int current = m_testRunner->CurrentStepIndex();
                m_testRunner->JumpToStep(std::max(0, current));
            }
            break;
        case kStop:
            if (m_pendingFailure) ResolvePendingFailure(StepFailureAction::Stop);
            if (m_testRunner) m_testRunner->Stop();
            break;
        case kPrev:
            if (m_testRunner) {
                const int current = m_testRunner->CurrentStepIndex();
                m_testRunner->JumpToStep(std::max(0, current - 1));
            }
            break;
        case kNext:
            if (m_testRunner) {
                const int current = m_testRunner->CurrentStepIndex();
                m_testRunner->JumpToStep(current + 1);
            }
            break;
        case kJump:
            if (m_testRunner) {
                wxArrayString names;
                for (long i = 0; i < stepList->GetItemCount() - 1; ++i)
                    names.Add(stepList->GetItemText(i));

                const int current = std::max(0, m_testRunner->CurrentStepIndex());
                int selected = current;
                if (ui::ChoiceDialog::Show(this, _("Jump to Step"), _("Select a step:"), names,
                                           current, selected)) {
                    m_testRunner->JumpToStep(selected);
                }
            }
            break;
        case kAdd:
            DoAddStep();
            break;
        case kCopy:
            DoCopyStep();
            break;
        case kEdit:
            DoEditStep();
            break;
        case kDelete:
            DoDeleteStep();
            break;
        case kBatch:
            DoBatchStep();
            break;
        case kUp:
            DoUpStep();
            break;
        case kDown:
            DoDownStep();
            break;
        default:
            break;
    }
    UpdateRuntimeButtons();
}

StepFailureAction SessionPanel::AskStepFailure(int stepIndex, const std::string& message) {
    if (!wxTheApp) return StepFailureAction::Stop;

    auto state = std::make_shared<PendingFailure>();
    m_pendingFailure = state;

    wxTheApp->CallAfter([this, stepIndex, message, state]() {
        ShowInlineFailure(m_activeStepList, stepIndex, wxString::FromUTF8(message), state);
    });

    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->cv.wait_for(lock, std::chrono::minutes(5), [&]() { return state->done; })) {
        wxTheApp->CallAfter([this]() {
            m_pendingFailure.reset();
            HideInlineFailure();
        });
        return StepFailureAction::Stop;
    }
    return state->action;
}

void SessionPanel::ShowInlineFailure(wxListCtrl* list, int stepIndex, const wxString& message,
                                     std::shared_ptr<PendingFailure> state) {
    if (!list || !state) return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->done) return;
    }
    m_pendingFailure = state;
    m_failureWaitStart = std::chrono::steady_clock::now();

    const bool autoMode = (list == m_autoStepList);
    wxStaticText* label = autoMode ? m_autoFailLabel : m_interactiveFailLabel;

    if (label) {
        label->SetLabel(wxString::Format(
            _("Step %d failed: %s\nRight-click the failed step, or use Run Controls, to Retry or "
              "Stop."),
            stepIndex + 1, message));
        label->Wrap(620);
        label->Show();
        label->GetParent()->Layout();
    }

    HighlightRunningStep(list, stepIndex);
    list->SetItemBackgroundColour(stepIndex, wxColour(254, 242, 242));
    UpdateRuntimeButtons();
}

void SessionPanel::HideInlineFailure() {
    auto hide = [](wxStaticText* label) {
        if (label) {
            label->SetLabel("");
            label->Hide();
            if (label->GetParent()) label->GetParent()->Layout();
        }
    };
    hide(m_autoFailLabel);
    hide(m_interactiveFailLabel);
}

void SessionPanel::ResolvePendingFailure(StepFailureAction action) {
    auto state = m_pendingFailure;
    if (!state) return;

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->action = action;
        state->done = true;
    }
    state->cv.notify_all();
    m_pendingFailure.reset();

    if (m_runTimingActive && m_failureWaitStart != std::chrono::steady_clock::time_point{}) {
        const auto now = std::chrono::steady_clock::now();
        m_extraWaitMs +=
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_failureWaitStart).count();
        m_failureWaitStart = {};
        UpdateRunTiming(m_activeStepList);
    }

    HideInlineFailure();
    UpdateRuntimeButtons();
}

void SessionPanel::BuildTestFlowUi(wxPanel* parent, bool isAutoMode) {
    auto* main = new wxBoxSizer(wxVERTICAL);

    auto* card = new ui::CardPanel(parent);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* progressRow = new wxBoxSizer(wxHORIZONTAL);
    auto* progressTitle = new wxStaticText(card, wxID_ANY, _("Test Progress") + ":");
    wxFont hf = progressTitle->GetFont();
    hf.SetPointSize(10);
    hf.MakeBold();
    progressTitle->SetFont(hf);
    progressTitle->SetForegroundColour(kSubTextColor);

    auto* progressBar = new ui::ProgressBar(card);
    progressBar->SetMinSize(wxSize(120, 22));
    progressBar->SetMaxSize(wxSize(120, 22));

    auto* ratioLabel = new wxStaticText(card, wxID_ANY, "0 / 0");
    ratioLabel->SetMinSize(wxSize(60, -1));
    ratioLabel->SetForegroundColour(kTextColor);

    auto* timeLabel =
        new wxStaticText(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    timeLabel->SetForegroundColour(kSubTextColor);

    progressRow->Add(progressTitle, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    progressRow->Add(progressBar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    progressRow->Add(ratioLabel, 0, wxALIGN_CENTER_VERTICAL);
    cardSizer->Add(progressRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    cardSizer->Add(timeLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    auto* failLabel = new wxStaticText(card, wxID_ANY, "");
    failLabel->SetForegroundColour(wxColour(198, 40, 40));
    failLabel->Wrap(620);
    failLabel->Hide();
    cardSizer->Add(failLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* stepList = new wxListCtrl(card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxLC_REPORT | wxBORDER_NONE);
    stepList->SetBackgroundColour(kCardBgColor);
    stepList->InsertColumn(0, _("Step Name"), wxLIST_FORMAT_LEFT, 220);
    stepList->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 80);
    stepList->InsertColumn(2, _("Details"), wxLIST_FORMAT_LEFT, 320);
    cardSizer->Add(stepList, 1, wxEXPAND | wxALL, 8);

    card->SetSizer(cardSizer);
    main->Add(card, 1, wxEXPAND | wxALL, 6);
    parent->SetSizer(main);

    if (isAutoMode) {
        m_autoStepList = stepList;
        m_autoProgressBar = progressBar;
        m_autoRatioLabel = ratioLabel;
        m_autoTimeLabel = timeLabel;
        m_autoFailLabel = failLabel;
    } else {
        m_interactiveStepList = stepList;
        m_interactiveProgressBar = progressBar;
        m_interactiveRatioLabel = ratioLabel;
        m_interactiveTimeLabel = timeLabel;
        m_interactiveFailLabel = failLabel;
    }

    stepList->Bind(wxEVT_LEFT_DOWN, [this, stepList](wxMouseEvent& evt) {
        m_activeContext = ActiveContext::Steps;
        int flags;
        long item = stepList->HitTest(evt.GetPosition(), flags, nullptr);
        if (item != -1 && item == stepList->GetItemCount() - 1) {
            CallAfter([this]() {
                m_activeContext = ActiveContext::Steps;
                DoAddStep();
            });
            return;
        }
        OnListLeftDown(stepList, evt.GetPosition());
        evt.Skip();
    });
    stepList->Bind(wxEVT_MOTION, [this, stepList](wxMouseEvent& evt) {
        OnListMotion(stepList, evt.GetPosition());
        evt.Skip();
    });
    stepList->Bind(wxEVT_LEFT_UP, [this, stepList](wxMouseEvent& evt) {
        OnListLeftUp(stepList, evt.GetPosition());
        evt.Skip();
    });
    stepList->Bind(wxEVT_MOUSE_CAPTURE_LOST,
                   [this](wxMouseCaptureLostEvent&) { CancelListDrag(); });

    stepList->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this, stepList](wxListEvent& evt) {
        long item = evt.GetIndex();
        if (item == stepList->GetItemCount() - 1) {
            DoAddStep();
            return;
        }
        m_activeContext = ActiveContext::Steps;
        DoEditStep();
    });
    stepList->Bind(wxEVT_CONTEXT_MENU, [this, stepList](wxContextMenuEvent& evt) {
        ShowStepContextMenu(stepList, evt.GetPosition());
    });

    stepList->Bind(wxEVT_LIST_ITEM_SELECTED, [this, stepList](wxListEvent& evt) {
        if (evt.GetIndex() == stepList->GetItemCount() - 1) {
            stepList->SetItemState(evt.GetIndex(), 0, wxLIST_STATE_SELECTED);
            return;
        }
        m_activeContext = ActiveContext::Steps;
        UpdateToolbarState();
    });
    stepList->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) { UpdateToolbarState(); });

    stepList->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& evt) {
        if (evt.ControlDown()) {
            if (evt.GetKeyCode() == 'N')
                DoAddStep();
            else if (evt.GetKeyCode() == 'C' || evt.GetKeyCode() == 'V')
                DoCopyStep();
            else if (evt.GetKeyCode() == 'E')
                DoEditStep();
            else
                evt.Skip();
        } else if (evt.GetKeyCode() == WXK_DELETE)
            DoDeleteStep();
        else
            evt.Skip();
    });

    if (m_sessionConfig) {
        const auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                      : m_sessionConfig->interactiveMode.testFlow;
        RefreshStepList(stepList, flow);
    }
}

void SessionPanel::DoAddStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    if (!flow.is_array()) flow = nlohmann::json::array();
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList) return;

    if (stepList->GetItemCount() > 0) {
        stepList->SetItemState(stepList->GetItemCount() - 1, 0, wxLIST_STATE_SELECTED);
    }

    std::string baseName = "New Step";
    std::string defName = baseName;
    int idx = 1;
    auto hasName = [&](const std::string& n) {
        for (const auto& s : flow)
            if (s.value("name", "") == n) return true;
        return false;
    };
    while (hasName(defName)) defName = baseName + " " + std::to_string(idx++);

    nlohmann::json initial;
    initial["name"] = defName;

    std::vector<std::string> varNames;
    std::map<std::string, std::string> varTypes;
    for (const auto& [n, v] : m_sessionConfig->variables) {
        varNames.push_back(n);
        varTypes[n] = v.type;
    }

    TestStepEditDialog dlg(this, initial, varNames, varTypes);
    if (dlg.ShowModal() == wxID_OK) {
        nlohmann::json step = dlg.GetStepJson();
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == stepList->GetItemCount() - 1) sel = -1;
        int insertPos = (sel != -1) ? static_cast<int>(sel) + 1 : static_cast<int>(flow.size());
        flow.insert(flow.begin() + insertPos, step);
        RefreshStepList(stepList, flow);
        stepList->SetItemState(insertPos, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        stepList->EnsureVisible(insertPos);
        UpdateToolbarState();
    }
}

void SessionPanel::DoCopyStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == stepList->GetItemCount() - 1) return;
    if (sel >= static_cast<long>(flow.size())) return;

    nlohmann::json copyStep = flow[sel];
    copyStep["name"] = copyStep.value("name", "Step") + " Copy";
    flow.insert(flow.begin() + sel + 1, copyStep);
    RefreshStepList(stepList, flow);
    stepList->SetItemState(sel + 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    stepList->EnsureVisible(sel + 1);
    UpdateToolbarState();
}

void SessionPanel::DoEditStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == stepList->GetItemCount() - 1) return;
    if (sel >= static_cast<long>(flow.size())) return;

    std::vector<std::string> varNames;
    std::map<std::string, std::string> varTypes;
    for (const auto& [n, v] : m_sessionConfig->variables) {
        varNames.push_back(n);
        varTypes[n] = v.type;
    }

    TestStepEditDialog dlg(this, flow[sel], varNames, varTypes);
    if (dlg.ShowModal() == wxID_OK) {
        flow[sel] = dlg.GetStepJson();
        RefreshStepList(stepList, flow);
        stepList->SetItemState(sel, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        UpdateToolbarState();
    }
}

void SessionPanel::DoDeleteStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    std::vector<long> toDelete;
    long item = -1;
    while ((item = stepList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1) {
        if (item != stepList->GetItemCount() - 1) toDelete.push_back(item);
    }
    if (toDelete.empty()) return;

    for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it)
        if (*it < static_cast<long>(flow.size())) flow.erase(flow.begin() + *it);
    RefreshStepList(stepList, flow);
    UpdateToolbarState();
}

void SessionPanel::DoBatchStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel == stepList->GetItemCount() - 1) {
        ShowMsg(_("Please select a base step first."), _("Notice"), wxOK);
        return;
    }
    if (sel >= static_cast<long>(flow.size())) return;

    wxPoint dialogDragStart;
    wxPoint dialogWinStart;
    wxDialog dlg(this, wxID_ANY, _("Batch Generate Steps"), wxDefaultPosition, wxDefaultSize,
                 wxBORDER_NONE | wxRESIZE_BORDER);
    dlg.SetBackgroundColour(ui::kBgColor);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    auto* header = ui::CreateDialogHeader(&dlg, _("Batch Generate Steps"), dialogDragStart,
                                          dialogWinStart, [&dlg]() { dlg.EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    auto* card = new ui::CardPanel(&dlg);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 10, 12);
    grid->AddGrowableCol(1, 1);

    auto addRow = [&](const wxString& label, wxWindow* ctrl) {
        auto* text = new wxStaticText(card, wxID_ANY, label);
        text->SetForegroundColour(ui::kTextColor);
        grid->Add(text, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(ctrl, 1, wxEXPAND);
    };

    auto* countCtrl = new wxSpinCtrl(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 1, 10000, 10);
    addRow(_("Repeat Count:"), countCtrl);

    auto* intervalCtrl = new wxSpinCtrl(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 0, 600000, 1000);
    addRow(_("Interval (ms):"), intervalCtrl);

    auto* incCtrl = new wxSpinCtrl(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS, -1000000, 1000000, 1);
    addRow(_("Auto Increment/Decrement:"), incCtrl);

    cardSizer->Add(grid, 1, wxEXPAND | wxALL, 16);
    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnCancel = new ui::FlatButton(&dlg, _("Cancel"), ui::kBtnGrayBg, ui::kTextColor,
                                         [&dlg]() { dlg.EndModal(wxID_CANCEL); });
    auto* btnOk = new ui::FlatButton(&dlg, _("OK"), ui::kThemeBlue, *wxWHITE,
                                     [&dlg]() { dlg.EndModal(wxID_OK); });
    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->Add(btnOk, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

    dlg.SetSizerAndFit(mainSizer);
    dlg.SetMinSize(dlg.FromDIP(wxSize(380, 300)));
    dlg.CentreOnParent();

    if (dlg.ShowModal() == wxID_OK) {
        const int repeat = countCtrl->GetValue();
        const int interval = intervalCtrl->GetValue();
        const int autoInc = incCtrl->GetValue();
        const nlohmann::json baseStep = flow[sel];
        const std::string baseValue = baseStep.value("value", "");
        long long baseValInt = 0;
        bool isInt = false;
        if (baseStep.value("action", "") == "write" || baseStep.value("action", "") == "send") {
            try {
                baseValInt = std::stoll(baseValue);
                isInt = true;
            } catch (...) {
            }
        }
        for (int i = 1; i < repeat; ++i) {
            nlohmann::json subStep = baseStep;
            subStep["name"] = baseStep.value("name", "Step") + " (" + std::to_string(i + 1) + ")";
            subStep["wait_ms"] = interval;
            if (isInt && autoInc != 0)
                subStep["value"] = std::to_string(baseValInt + (i * autoInc));
            flow.insert(flow.begin() + sel + i, subStep);
        }
        RefreshStepList(stepList, flow);
        UpdateToolbarState();
    }
}

void SessionPanel::DoUpStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel <= 0 || sel == stepList->GetItemCount() - 1) return;
    if (sel >= static_cast<long>(flow.size())) return;

    std::swap(flow[sel], flow[sel - 1]);
    RefreshStepList(stepList, flow);
    stepList->SetItemState(sel - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    stepList->EnsureVisible(sel - 1);
    UpdateToolbarState();
}

void SessionPanel::DoDownStep() {
    if (!m_sessionConfig || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array()) return;

    long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1 || sel >= static_cast<long>(flow.size()) - 1) return;

    std::swap(flow[sel], flow[sel + 1]);
    RefreshStepList(stepList, flow);
    stepList->SetItemState(sel + 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    stepList->EnsureVisible(sel + 1);
    UpdateToolbarState();
}

void SessionPanel::OnTbAction(wxCommandEvent&) {
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
    UpdateToolbarState();
}

void SessionPanel::OnTbConfigure(wxCommandEvent&) {
    if (m_onRequestConfigure) m_onRequestConfigure();
}

void SessionPanel::OnTbRecord(wxCommandEvent&) {
    if (m_isRecording) {
        m_isRecording = false;
        UpdateToolbarState();
        return;
    }
    if (!m_sessionConfig) return;

    wxArrayString choices;
    std::vector<std::string> names;
    for (const auto& [name, var] : m_sessionConfig->variables) {
        if (!var.enabled) continue;
        names.push_back(name);
        choices.Add(
            wxString::Format("%s (%s)", wxString::FromUTF8(name), wxString::FromUTF8(var.type)));
    }
    if (choices.IsEmpty()) {
        ShowMsg(_("No enabled variables to record."), _("Notice"), wxOK);
        return;
    }

    std::vector<int> initial;
    for (unsigned int i = 0; i < choices.GetCount(); ++i) initial.push_back(static_cast<int>(i));

    std::vector<int> selected;
    if (ui::CheckListDialog::Show(this, _("Record Settings"),
                                  _("Select variables to record (numeric: chart, text: details):"),
                                  choices, initial, selected)) {
        if (selected.empty()) {
            ShowMsg(_("Please select at least one variable."), _("Notice"), wxOK);
            return;
        }
        m_loggingVars.clear();
        for (int sel : selected) {
            if (sel >= 0 && sel < static_cast<int>(names.size())) m_loggingVars.insert(names[sel]);
        }
        m_isRecording = true;
        if (m_chartPanel) m_chartPanel->Clear();
        UpdateToolbarState();
    }
}

void SessionPanel::OnTbHistory(wxCommandEvent&) {
    if (!m_sessionConfig) return;
    std::vector<std::string> varNames;
    for (const auto& [n, v] : m_sessionConfig->variables)
        if (v.access != "WO") varNames.push_back(n);
    if (varNames.empty()) {
        ShowMsg(_("No readable variables found."), _("Notice"), wxOK);
        return;
    }

    HistoryDialog dlg(this, &m_db, m_historyKey, varNames);
    dlg.ShowModal();
}

void SessionPanel::OnTbAdd(wxCommandEvent&) {
    if (m_activeContext == ActiveContext::Variables)
        DoAddVariable();
    else
        DoAddStep();
}

void SessionPanel::OnTbCopy(wxCommandEvent&) {
    if (m_activeContext == ActiveContext::Variables)
        DoCopyVariable();
    else
        DoCopyStep();
}

void SessionPanel::OnTbEdit(wxCommandEvent&) {
    if (m_activeContext == ActiveContext::Variables)
        DoEditVariable();
    else
        DoEditStep();
}

void SessionPanel::OnTbDelete(wxCommandEvent&) {
    if (m_activeContext == ActiveContext::Variables)
        DoDeleteVariable();
    else
        DoDeleteStep();
}

void SessionPanel::OnTbRunStop(wxCommandEvent&) {
    if (m_testRunner) {
        if (m_pendingFailure) ResolvePendingFailure(StepFailureAction::Stop);
        m_testRunner->Stop();
        return;
    }

    if (!m_sessionConfig || !m_session || !m_modeNotebook) return;
    const bool isAutoMode = (m_modeNotebook->GetSelection() == 2);
    auto& flow =
        isAutoMode ? m_sessionConfig->autoMode.testFlow : m_sessionConfig->interactiveMode.testFlow;
    wxListCtrl* stepList = isAutoMode ? m_autoStepList : m_interactiveStepList;
    if (!stepList || !flow.is_array() || flow.empty()) return;
    m_activeStepList = stepList;
    m_pendingFailure.reset();

    // 运行总是切到对应的流程页，方便看到当前步骤高亮和进度
    if (m_modeNotebook) {
        const int targetPage = isAutoMode ? 2 : 1;
        if (m_modeNotebook->GetSelection() != targetPage) m_modeNotebook->SetSelection(targetPage);
    }

    std::vector<ITestStepPtr> steps;
    for (const auto& stepJson : flow) {
        std::string type = stepJson.value("action", "");
        ITestStepPtr step;
        if (type == "delay")
            step = std::make_unique<DelayStep>(stepJson);
        else
            step = m_protocol->CreateTestStep(type, stepJson);

        if (step)
            steps.push_back(std::move(step));
        else {
            ShowMsg(
                wxString::Format(_("Protocol '%s' does not support action '%s'."),
                                 wxString::FromUTF8(m_protocol->Id()), wxString::FromUTF8(type)),
                _("Unsupported Action"), wxOK | wxICON_ERROR);
            return;
        }
    }
    if (steps.empty()) {
        ShowMsg(_("No valid steps."), _("Error"), wxOK);
        return;
    }

    for (int i = 0; i < stepList->GetItemCount() - 1; ++i) {
        stepList->SetItem(i, 1, _("Pending"));
        stepList->SetItemTextColour(i, kSubTextColor);
        stepList->SetItemBackgroundColour(i, kCardBgColor);
    }
    ClearRunningHighlight();
    HideInlineFailure();
    m_plannedWaitMs = 0;
    for (const auto& stepJson : flow) m_plannedWaitMs += stepJson.value("wait_ms", 0);
    m_extraWaitMs = 0;
    m_runStart = std::chrono::steady_clock::now();
    m_runTimingActive = true;
    m_failureWaitStart = {};
    if (m_runTimer) m_runTimer->Start(500);
    UpdateStepProgress(stepList, 0, wxString::Format("0 / %d", static_cast<int>(flow.size())));

    m_testRunner =
        new TestRunner(m_protocol, m_session, std::move(steps), m_sessionConfig->variables);
    m_testRunner->SetRunMode(isAutoMode ? TestRunMode::Auto : TestRunMode::SingleStep);

    m_testRunner->OnStepUpdated([this, stepList](int index, StepStatus status, const std::string&) {
        wxTheApp->CallAfter([this, stepList, index, status]() {
            if (index < 0 || index >= stepList->GetItemCount() - 1) return;
            wxString text;
            wxColour colour = kSubTextColor;
            switch (status) {
                case StepStatus::Running:
                    text = _("Running");
                    colour = kThemeColor;
                    break;
                case StepStatus::Passed:
                    text = _("Passed");
                    colour = wxColour(46, 125, 50);
                    break;
                case StepStatus::Failed:
                    text = _("Failed");
                    colour = wxColour(198, 40, 40);
                    break;
                case StepStatus::Skipped:
                    text = _("Skipped");
                    colour = wxColour(180, 83, 9);
                    break;
            }
            stepList->SetItem(index, 1, text);
            stepList->SetItemTextColour(index, colour);

            const int total = std::max(0, static_cast<int>(stepList->GetItemCount()) - 1);
            switch (status) {
                case StepStatus::Running:
                    HideInlineFailure();
                    stepList->SetItemBackgroundColour(index, kCardBgColor);
                    HighlightRunningStep(stepList, index);
                    UpdateStepProgress(stepList, index,
                                       wxString::Format(_("Running %d / %d"), index + 1, total));
                    break;
                case StepStatus::Passed:
                    stepList->SetItemBackgroundColour(index, kCardBgColor);
                    UpdateStepProgress(stepList, index + 1,
                                       wxString::Format(_("Completed %d / %d"), index + 1, total));
                    break;
                case StepStatus::Failed:
                    stepList->SetItemBackgroundColour(index, wxColour(254, 242, 242));
                    UpdateStepProgress(stepList, index,
                                       wxString::Format(_("Step %d failed"), index + 1));
                    break;
                case StepStatus::Skipped:
                    stepList->SetItemBackgroundColour(index, wxColour(255, 251, 235));
                    UpdateStepProgress(stepList, index + 1,
                                       wxString::Format(_("Skipped %d / %d"), index + 1, total));
                    break;
            }
            stepList->EnsureVisible(index);
            RefreshVariableList();
            // 同步手动操作区显示的值，避免运行后仍显示旧值
            if (!m_selectedVarName.empty() && m_sessionConfig &&
                m_sessionConfig->variables.count(m_selectedVarName)) {
                if (m_varOpControls.count("__value")) {
                    if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])) {
                        if (!txt->HasFocus()) {
                            txt->ChangeValue(wxString::FromUTF8(
                                m_sessionConfig->variables[m_selectedVarName].currentValue));
                            ValidateValueInput();
                        }
                    }
                }
                if (m_varOpControls.count("__raw_bytes")) {
                    if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"])) {
                        txt->ChangeValue(wxString::FromUTF8(
                            m_sessionConfig->variables[m_selectedVarName].rawBytes));
                    }
                }
            }
        });
    });

    m_testRunner->OnStepFailed(
        [this](int index, const std::string& msg) { return AskStepFailure(index, msg); });

    m_testRunner->OnStepLog([this](const std::string& msg) {
        CallAfter([this, msg]() { AppendRunnerLog(wxString::FromUTF8(msg)); });
    });

    m_testRunner->OnDataRecorded([this](const std::string& varName, const std::string& val) {
        // 数据记录回调在测试线程触发，DB/图表只能在 UI 线程访问
        CallAfter([this, varName, val]() { RecordVariableValue(varName, val); });
    });

    m_testRunner->OnFinished([this, stepList](bool overallSuccess) {
        CallAfter([this, stepList, overallSuccess]() {
            const int total =
                stepList ? std::max(0, static_cast<int>(stepList->GetItemCount()) - 1) : 0;
            const int completed = (m_runningStepRow >= 0) ? m_runningStepRow + 1 : 0;
            ClearRunningHighlight();
            HideInlineFailure();
            m_pendingFailure.reset();
            m_activeStepList = nullptr;
            UpdateStepProgress(stepList, overallSuccess ? total : completed,
                               overallSuccess ? _("Finished") : _("Stopped"));
            m_runTimingActive = false;
            if (m_runTimer) m_runTimer->Stop();
            AppendLogMessage(overallSuccess ? _("Test Finished Successfully") : _("Test Failed"),
                             !overallSuccess);
            if (m_testRunner) {
                // 先 join 线程再释放，避免回调尚未返回时对象已被销毁
                m_testRunner->Stop();
                delete m_testRunner;
                m_testRunner = nullptr;
            }
            UpdateToolbarState();
            UpdateRuntimeButtons();
        });
    });

    m_testRunner->OnWaitForNextStep([this, stepList](int nextIndex) {
        wxTheApp->CallAfter([this, stepList, nextIndex]() {
            if (nextIndex < stepList->GetItemCount() - 1) {
                for (long i = 0; i < stepList->GetItemCount() - 1; ++i)
                    stepList->SetItemState(i, 0, wxLIST_STATE_SELECTED);
                stepList->SetItemState(nextIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                stepList->EnsureVisible(nextIndex);
            }
            int ret = ShowMsg(
                wxString::Format(_("Step %d finished.\n\nReady to execute Step %d. Continue?"),
                                 nextIndex - 1, nextIndex),
                _("Interactive Mode"), wxYES_NO | wxICON_QUESTION, this);
            if (m_testRunner) {
                if (ret == wxYES)
                    m_testRunner->Continue();
                else
                    m_testRunner->Stop();
            }
        });
    });

    UpdateToolbarState();
    m_testRunner->Start();
    UpdateRuntimeButtons();
}

void SessionPanel::OnTbBatch(wxCommandEvent&) {
    DoBatchStep();
}
void SessionPanel::OnTbUp(wxCommandEvent&) {
    DoUpStep();
}
void SessionPanel::OnTbDown(wxCommandEvent&) {
    DoDownStep();
}

void SessionPanel::OnClear(wxCommandEvent&) {
    if (m_commonLogCtrl) m_commonLogCtrl->Clear();
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

    UpdateToolbarState();
    Layout();
}

void SessionPanel::UpdateStateLabel(SessionState st) {
    if (!m_statusLabel) return;
    bool isServer = m_streamSession && m_streamSession->IsServerMode();
    switch (st) {
        case SessionState::Idle:
            m_statusLabel->SetLabel(_("Idle"));
            m_statusLabel->SetForegroundColour(wxColour(128, 128, 128));
            break;
        case SessionState::Connecting:
            m_statusLabel->SetLabel(_("Connecting..."));
            m_statusLabel->SetForegroundColour(wxColour(255, 152, 0));
            break;
        case SessionState::Reconnecting:
            m_statusLabel->SetLabel(_("Reconnecting..."));
            m_statusLabel->SetForegroundColour(wxColour(255, 152, 0));
            break;
        case SessionState::Connected:
            if (m_streamSession && isServer)
                m_statusLabel->SetLabel(m_clientCount > 0
                                            ? wxString::Format(_("Connected (%d)"), m_clientCount)
                                            : _("Listening"));
            else
                m_statusLabel->SetLabel(_("Connected"));
            m_statusLabel->SetForegroundColour(wxColour(76, 175, 80));
            break;
        case SessionState::Disconnected:
            m_statusLabel->SetLabel(_("Disconnected"));
            m_statusLabel->SetForegroundColour(wxColour(244, 67, 54));
            break;
    }
}

void SessionPanel::UpdateActionButton(SessionState) {
    // 工具栏按钮现在是纯图标 + ToolTip，动作文字不再画在按钮上。
    // 状态变化时只需要同步图标、提示与可用性。
    UpdateToolbarState();
}

void SessionPanel::UpdateClientsLabel() {
    if (m_clientsLabel) m_clientsLabel->SetLabel(wxString::Format(_("Clients: %d"), m_clientCount));
}

void SessionPanel::BindStreamCallbacks() {
    m_streamSession->OnMessage([this](int cid, const std::vector<uint8_t>& msg) {
        auto copy = msg;
        CallAfter([this, cid, copy = std::move(copy)]() {
            m_rxBytes += copy.size();
            if (m_rxLabel)
                m_rxLabel->SetLabel(
                    wxString::Format(_("Received: %llu bytes"), (unsigned long long)m_rxBytes));

            std::string parsedValue;
            std::string varType = "string";
            std::string varName = m_selectedVarName.empty() ? "Unknown" : m_selectedVarName;

            if (!m_selectedVarName.empty() && m_sessionConfig &&
                m_sessionConfig->variables.count(m_selectedVarName)) {
                auto& var = m_sessionConfig->variables[m_selectedVarName];
                varType = var.type;
                if (BytesParser::Parse(var.type, copy, parsedValue)) {
                    var.currentValue = parsedValue;
                    wxString rawStr;
                    for (uint8_t b : copy) rawStr += wxString::Format("%02X ", b);
                    var.rawBytes = std::string(rawStr.Trim().utf8_str());
                    RefreshVariableList();
                    if (m_varOpControls.count("__value"))
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"])) {
                            txt->ChangeValue(wxString::FromUTF8(var.currentValue));
                            ValidateValueInput();
                        }
                    if (m_varOpControls.count("__raw_bytes"))
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                            txt->ChangeValue(rawStr.Trim());
                    RecordVariableValue(m_selectedVarName, var.currentValue);
                } else {
                    parsedValue = "<Parse Failed>";
                }
            } else {
                parsedValue = "<No Variable Selected>";
            }

            AppendUnifiedLog(true, varName, varType, parsedValue, copy, cid);
        });
    });

    m_streamSession->OnReconnectAttempt([this](int attempt) {
        CallAfter([this, attempt]() {
            AppendLogMessage(wxString::Format(_("Reconnecting (attempt %d) ..."), attempt));
        });
    });

    m_streamSession->OnClientConnected([this](int cid) {
        CallAfter([this, cid]() {
            ++m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            AppendLogMessage(wxString::Format(_("Client #%d connected"), cid));
        });
    });

    m_streamSession->OnClientDisconnected([this](int cid) {
        CallAfter([this, cid]() {
            if (m_clientCount > 0) --m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            AppendLogMessage(wxString::Format(_("Client #%d disconnected"), cid));
        });
    });

    m_streamSession->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            RefreshFromSession();
            if (m_onStateUpdated) m_onStateUpdated(st);
        });
    });
}

void SessionPanel::BindTransactionCallbacks() {
    m_txSession->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            RefreshFromSession();
            if (m_onStateUpdated) m_onStateUpdated(st);
        });
    });
    m_txSession->OnLog([this](const std::string& msg) {
        CallAfter([this, msg]() { AppendRunnerLog(wxString::FromUTF8(msg)); });
    });
}

void SessionPanel::BindMqttCallbacks() {
    m_mqttSession->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            RefreshFromSession();
            if (m_onStateUpdated) m_onStateUpdated(st);
        });
    });

    m_mqttSession->OnMqttMessage([this](const std::string& topic, const std::string& payload) {
        CallAfter([this, topic, payload]() {
            m_rxBytes += payload.size();
            if (m_rxLabel)
                m_rxLabel->SetLabel(
                    wxString::Format(_("Received: %llu bytes"), (unsigned long long)m_rxBytes));

            std::vector<uint8_t> copy(payload.begin(), payload.end());
            AppendUnifiedLog(true, "Topic: " + topic, "mqtt", payload, copy);

            if (!m_selectedVarName.empty() && m_sessionConfig &&
                m_sessionConfig->variables.count(m_selectedVarName)) {
                auto& var = m_sessionConfig->variables[m_selectedVarName];
                std::string varTopic = var.params.value("topic", "test/topic");
                if (topic == varTopic) {
                    var.currentValue = payload;
                    wxString rawStr;
                    for (uint8_t b : copy) rawStr += wxString::Format("%02X ", b);
                    var.rawBytes = std::string(rawStr.Trim().utf8_str());
                    RefreshVariableList();
                    if (m_varOpControls.count("__value"))
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                            txt->ChangeValue(wxString::FromUTF8(payload));
                    if (m_varOpControls.count("__raw_bytes"))
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                            txt->ChangeValue(rawStr.Trim());
                    RecordVariableValue(m_selectedVarName, var.currentValue);
                }
            }
        });
    });
}

void SessionPanel::CmdAdd() {
    wxCommandEvent evt(wxEVT_BUTTON, idTbAdd);
    OnTbAdd(evt);
}

void SessionPanel::CmdCopy() {
    wxCommandEvent evt(wxEVT_BUTTON, idTbCopy);
    OnTbCopy(evt);
}

void SessionPanel::CmdEdit() {
    wxCommandEvent evt(wxEVT_BUTTON, idTbEdit);
    OnTbEdit(evt);
}

void SessionPanel::CmdDelete() {
    wxCommandEvent evt(wxEVT_BUTTON, idTbDelete);
    OnTbDelete(evt);
}

bool SessionPanel::HasSelection() const {
    if (m_activeContext == ActiveContext::Variables) {
        if (!m_varList) return false;
        long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        return sel != -1 && sel != m_varList->GetItemCount() - 1;
    }

    if (!m_modeNotebook) return false;
    wxListCtrl* list =
        (m_modeNotebook->GetSelection() == 2) ? m_autoStepList : m_interactiveStepList;
    if (!list) return false;
    long sel = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    return sel != -1 && sel != list->GetItemCount() - 1;
}

}  // namespace icu
