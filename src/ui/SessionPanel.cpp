#include "SessionPanel.h"

#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>

#include <algorithm>
#include <cstring>
#include <thread>

#include "TestStepEditDialog.h"
#include "VariableEditDialog.h"
#include "core/BytesParser.h"
#include "core/DelayStep.h"
#include "log/Log.h"
#include "settings/UISettings.h"  // ★ 引入全局设置
#include "spdlog/spdlog.h"

namespace icu {

namespace {
std::vector<uint8_t> EncodePayload(const std::string& type, const std::string& payload) {
    std::vector<uint8_t> bytes;
    if (type == "int8") {
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoi(payload)));
        } catch (...) {
        }
    } else if (type == "uint8") {
        try {
            bytes.push_back(static_cast<uint8_t>(std::stoul(payload)));
        } catch (...) {
        }
    } else if (type == "int16" || type == "uint16") {
        try {
            uint16_t v = static_cast<uint16_t>(std::stoul(payload));
            bytes.push_back((v >> 8) & 0xFF);
            bytes.push_back(v & 0xFF);
        } catch (...) {
        }
    } else if (type == "int" || type == "int32") {
        try {
            int32_t v = std::stoi(payload);
            bytes.resize(4);
            bytes[0] = (v >> 24) & 0xFF;
            bytes[1] = (v >> 16) & 0xFF;
            bytes[2] = (v >> 8) & 0xFF;
            bytes[3] = v & 0xFF;
        } catch (...) {
        }
    } else if (type == "uint" || type == "uint32") {
        try {
            uint32_t v = std::stoul(payload);
            bytes.resize(4);
            bytes[0] = (v >> 24) & 0xFF;
            bytes[1] = (v >> 16) & 0xFF;
            bytes[2] = (v >> 8) & 0xFF;
            bytes[3] = v & 0xFF;
        } catch (...) {
        }
    } else if (type == "float") {
        try {
            float v = std::stof(payload);
            uint32_t intVal;
            std::memcpy(&intVal, &v, sizeof(intVal));
            bytes.resize(4);
            bytes[0] = (intVal >> 24) & 0xFF;
            bytes[1] = (intVal >> 16) & 0xFF;
            bytes[2] = (intVal >> 8) & 0xFF;
            bytes[3] = intVal & 0xFF;
        } catch (...) {
        }
    } else if (type == "double") {
        try {
            double v = std::stod(payload);
            uint64_t intVal;
            std::memcpy(&intVal, &v, sizeof(intVal));
            bytes.resize(8);
            for (int i = 7; i >= 0; --i) bytes[7 - i] = (intVal >> (i * 8)) & 0xFF;
        } catch (...) {
        }
    } else if (type == "bool") {
        bytes.push_back((payload == "true" || payload == "1") ? 1 : 0);
    } else {
        bytes.assign(payload.begin(), payload.end());
    }
    return bytes;
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

    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    exePath.SetFullName("test_history.db");
    m_db.Open(std::string(exePath.GetFullPath().utf8_str()));

    wxString runPath = m_profilePath.IsEmpty() ? "Untitled" : m_profilePath;
    m_currentRunId = m_db.CreateTestRun(std::string(runPath.utf8_str()));

    BuildUi();

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
        m_testRunner = nullptr;
    }
    if (m_currentRunId != -1) {
        m_db.UpdateTestRunStatus(m_currentRunId, "CLOSED");
    }
    m_db.Close();
}

void SessionPanel::ApplyGlobalSettings() {
    auto& ui = settings::UISettings::Instance();
    if (m_hexDisplay) m_hexDisplay->SetValue(ui.GetHexDisplay());
    if (m_showTimestamp) m_showTimestamp->SetValue(ui.GetShowTimestamp());
    if (m_autoScroll) m_autoScroll->SetValue(ui.GetAutoScroll());
}

void SessionPanel::RecordData(const std::string& varName, double value) {
    if (!m_isRecording) return;
    if (!m_loggingVars.empty() && m_loggingVars.find(varName) == m_loggingVars.end()) return;

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    double timeSec = ms / 1000.0;

    if (m_currentRunId != -1) {
        m_db.InsertRunData(m_currentRunId, varName, value, timeSec);
    }
    if (m_chartPanel) {
        m_chartPanel->AddDataPointWithTime(varName, value, timeSec);
    }
}

void SessionPanel::AppendUnifiedLog(bool isRx, const std::string& varName, const std::string& type,
                                    const std::string& value, const std::vector<uint8_t>& rawBytes,
                                    int clientId) {
    if (!m_commonLogCtrl) return;
    wxString timeStr = wxDateTime::Now().Format("[%H:%M:%S.%l] ");

    wxString dirStr = isRx ? (clientId > 0 ? wxString::Format("<#%d ", clientId) : "< ") : "> ";
    wxString varStr = wxString::FromUTF8(varName);
    if (varStr.IsEmpty()) varStr = "Unknown";
    wxString typeStr = wxString::FromUTF8(type);
    wxString valStr = wxString::FromUTF8(value);

    wxString hexStr;
    for (uint8_t b : rawBytes) hexStr += wxString::Format("%02X ", b);
    hexStr = hexStr.Trim();

    wxString line;
    if (typeStr.IsEmpty() || typeStr == "Unknown") {
        line = timeStr + dirStr + wxString::Format("%s | HEX: %s\n", varStr, hexStr);
    } else {
        line = timeStr + dirStr +
               wxString::Format("%s (%s): %s | HEX: %s\n", varStr, typeStr, valStr, hexStr);
    }

    m_commonLogCtrl->AppendText(line);
    m_commonLogCtrl->SetInsertionPointEnd();
}

void SessionPanel::BuildUi() {
    auto* main = new wxBoxSizer(wxVERTICAL);
    BuildCommonUi(main);

    m_mainSplitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxSP_LIVE_UPDATE | wxSP_3DSASH);

    auto* varPanel = new wxPanel(m_mainSplitter);
    auto* varSizer = new wxBoxSizer(wxVERTICAL);
    auto* varBox = new wxStaticBoxSizer(wxVERTICAL, varPanel, _("Communication Variables"));

    m_varList = new wxListCtrl(varBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL);
    m_varList->EnableCheckBoxes(true);
    m_varList->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 120);
    m_varList->InsertColumn(1, _("Type"), wxLIST_FORMAT_LEFT, 60);
    m_varList->InsertColumn(2, _("Access"), wxLIST_FORMAT_LEFT, 60);
    m_varList->InsertColumn(3, _("Value"), wxLIST_FORMAT_LEFT, 100);

    m_varList->Bind(wxEVT_LIST_ITEM_SELECTED, &SessionPanel::OnVarSelected, this);
    m_varList->Bind(wxEVT_LIST_ITEM_CHECKED, &SessionPanel::OnItemChecked, this);
    m_varList->Bind(wxEVT_LIST_ITEM_UNCHECKED, &SessionPanel::OnItemChecked, this);
    varBox->Add(m_varList, 1, wxEXPAND | wxALL, 4);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    m_addVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Add"));
    m_copyVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Copy"));
    m_editVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Edit"));
    m_delVarBtn = new wxButton(varBox->GetStaticBox(), wxID_ANY, _("Delete"));

    m_addVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnAddVariable, this);
    m_copyVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnCopyVariable, this);
    m_editVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnEditVariable, this);
    m_delVarBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnDeleteVariable, this);

    btnSizer->Add(m_addVarBtn, 0, wxALL, 2);
    btnSizer->Add(m_copyVarBtn, 0, wxALL, 2);
    btnSizer->Add(m_editVarBtn, 0, wxALL, 2);
    btnSizer->Add(m_delVarBtn, 0, wxALL, 2);
    varBox->Add(btnSizer, 0, wxALIGN_RIGHT);
    varSizer->Add(varBox, 1, wxEXPAND | wxALL, 5);
    varPanel->SetSizer(varSizer);
    RefreshVariableList();

    m_rightSplitter = new wxSplitterWindow(m_mainSplitter, wxID_ANY, wxDefaultPosition,
                                           wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3DSASH);

    m_modeNotebook = new wxNotebook(m_rightSplitter, wxID_ANY);
    m_manualPage = new wxPanel(m_modeNotebook);
    m_interactivePage = new wxPanel(m_modeNotebook);
    m_autoPage = new wxPanel(m_modeNotebook);
    m_chartPanel = new ChartPanel(m_modeNotebook);

    m_modeNotebook->AddPage(m_manualPage, _("Manual Mode"));
    m_modeNotebook->AddPage(m_interactivePage, _("Interactive Mode"));
    m_modeNotebook->AddPage(m_autoPage, _("Auto Mode"));
    m_modeNotebook->AddPage(m_chartPanel, _("Chart"));
    m_modeNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SessionPanel::OnNotebookPageChanged, this);

    auto* manualSizer = new wxBoxSizer(wxVERTICAL);
    m_varOpSizer = new wxStaticBoxSizer(wxVERTICAL, m_manualPage, _("Operation Request"));
    auto* hint = new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                  _("Please select a variable from the left list."));
    m_varOpSizer->Add(hint, 0, wxALL, 10);
    manualSizer->Add(m_varOpSizer, 0, wxEXPAND | wxALL, 5);

    if (m_streamSession || m_mqttSession)
        BuildStreamUi(manualSizer, m_manualPage);
    else if (m_txSession)
        BuildTransactionUi(manualSizer, m_manualPage);

    m_manualPage->SetSizer(manualSizer);

    BuildTestFlowUi(m_interactivePage, false);
    BuildTestFlowUi(m_autoPage, true);

    auto* logPanel = new wxPanel(m_rightSplitter);
    auto* logSizer = new wxBoxSizer(wxVERTICAL);
    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, logPanel, _("Communication Log"));
    auto* logToolbar = new wxBoxSizer(wxHORIZONTAL);
    m_clearBtn = new wxButton(logBox->GetStaticBox(), wxID_ANY, _("Clear"));
    m_clearBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnClear, this);
    logToolbar->AddStretchSpacer();
    logToolbar->Add(m_clearBtn, 0, wxALL, 4);
    logBox->Add(logToolbar, 0, wxEXPAND);

    m_commonLogCtrl = new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, "", wxDefaultPosition,
                                     wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    wxFont mono(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_commonLogCtrl->SetFont(mono);
    logBox->Add(m_commonLogCtrl, 1, wxEXPAND | wxALL, 4);
    logSizer->Add(logBox, 1, wxEXPAND);
    logPanel->SetSizer(logSizer);

    m_rightSplitter->SplitHorizontally(m_modeNotebook, logPanel, 400);
    m_rightSplitter->SetMinimumPaneSize(150);

    m_mainSplitter->SplitVertically(varPanel, m_rightSplitter, 320);
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
    btnSizer->Add(m_configBtn, 0, wxRIGHT, 8);

    m_recordBtn = new wxButton(this, wxID_ANY, _("Record Settings"));
    m_recordBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnRecordSettings, this);
    btnSizer->Add(m_recordBtn, 0, wxRIGHT, 8);

    m_historyBtn = new wxButton(this, wxID_ANY, _("History Query"));
    m_historyBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnHistoryQuery, this);
    btnSizer->Add(m_historyBtn, 0, wxRIGHT, 8);

    mainSizer->Add(btnSizer, 0, wxALL, 10);
}

void SessionPanel::BuildStreamUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    if (m_streamSession && m_streamSession->IsServerMode())
        m_clientsLabel->Show();
    else
        m_clientsLabel->Hide();

    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Data Log"));
    auto* logToolbar = new wxBoxSizer(wxHORIZONTAL);

    m_hexDisplay = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("HEX Display"));
    m_showTimestamp = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Timestamp"));
    m_autoScroll = new wxCheckBox(logBox->GetStaticBox(), wxID_ANY, _("Auto Scroll"));

    auto& ui = settings::UISettings::Instance();
    m_hexDisplay->SetValue(ui.GetHexDisplay());
    m_showTimestamp->SetValue(ui.GetShowTimestamp());
    m_autoScroll->SetValue(ui.GetAutoScroll());

    m_hexDisplay->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& evt) {
        settings::UISettings::Instance().SetHexDisplay(evt.IsChecked());
    });
    m_showTimestamp->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& evt) {
        settings::UISettings::Instance().SetShowTimestamp(evt.IsChecked());
    });
    m_autoScroll->Bind(wxEVT_CHECKBOX, [](wxCommandEvent& evt) {
        settings::UISettings::Instance().SetAutoScroll(evt.IsChecked());
    });

    m_clearBtn = new wxButton(logBox->GetStaticBox(), wxID_ANY, _("Clear"));
    m_clearBtn->Bind(wxEVT_BUTTON, &SessionPanel::OnClear, this);

    logToolbar->Add(m_hexDisplay, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->Add(m_showTimestamp, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->Add(m_autoScroll, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logToolbar->AddStretchSpacer();
    logToolbar->Add(m_clearBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    logBox->Add(logToolbar, 0, wxEXPAND);
    mainSizer->Add(logBox, 1, wxEXPAND | wxALL, 5);
}

void SessionPanel::BuildTransactionUi(wxBoxSizer* mainSizer, wxWindow* parentWindow) {
    m_rxLabel->Hide();
    m_txLabel->Hide();
    m_clientsLabel->Hide();
    auto* logBox = new wxStaticBoxSizer(wxVERTICAL, parentWindow, _("Result Log"));
    wxFont mono(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    logBox->Add(new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, "", wxDefaultPosition,
                               wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2),
                1, wxEXPAND | wxALL, 4);
    mainSizer->Add(logBox, 1, wxEXPAND | wxALL, 5);
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

    grid->Add(new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Value:")), 0,
              wxALIGN_CENTER_VERTICAL);

    auto* valueCtrl = new wxTextCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY,
                                     wxString::FromUTF8(var.currentValue));
    valueCtrl->Bind(wxEVT_TEXT, [this, valueCtrl](wxCommandEvent&) {
        if (!m_selectedVarName.empty() && m_sessionConfig) {
            std::string text = std::string(valueCtrl->GetValue().utf8_str());
            m_sessionConfig->variables[m_selectedVarName].currentValue = text;

            wxString rawStr;
            if (m_streamSession || m_mqttSession) {
                std::vector<uint8_t> bytes =
                    EncodePayload(m_sessionConfig->variables[m_selectedVarName].type, text);
                for (uint8_t b : bytes) rawStr += wxString::Format("%02X ", b);
                rawStr = rawStr.Trim();
            } else if (m_txSession) {
                try {
                    int val = std::stoi(text);
                    uint16_t reg = static_cast<uint16_t>(val);
                    rawStr = wxString::Format("%02X %02X", (reg >> 8) & 0xFF, reg & 0xFF);
                } catch (...) {
                }
            }
            m_sessionConfig->variables[m_selectedVarName].rawBytes = std::string(rawStr.utf8_str());

            if (m_varOpControls.count("__raw_bytes")) {
                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"])) {
                    txt->ChangeValue(rawStr);
                }
            }
        }
    });
    if (var.access == "RO") {
        valueCtrl->SetEditable(false);
        valueCtrl->SetBackgroundColour(wxColour(240, 240, 240));
    }
    grid->Add(valueCtrl, 1, wxEXPAND);
    m_varOpControls["__value"] = valueCtrl;

    grid->Add(new wxStaticText(m_varOpSizer->GetStaticBox(), wxID_ANY, _("Raw Bytes:")), 0,
              wxALIGN_CENTER_VERTICAL);
    auto* rawCtrl =
        new wxTextCtrl(m_varOpSizer->GetStaticBox(), wxID_ANY, wxString::FromUTF8(var.rawBytes),
                       wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    rawCtrl->SetBackgroundColour(wxColour(240, 240, 240));
    grid->Add(rawCtrl, 1, wxEXPAND);
    m_varOpControls["__raw_bytes"] = rawCtrl;

    m_varOpSizer->Add(grid, 1, wxEXPAND | wxALL, 8);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    if (m_streamSession || m_mqttSession) {
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
    m_varOpSizer->Layout();
    m_manualPage->Layout();
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
        m_varList->CheckItem(item, var.enabled);
        if (!var.enabled) m_varList->SetItemTextColour(item, wxColour(150, 150, 150));
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

void SessionPanel::OnItemChecked(wxListEvent& evt) {
    long item = evt.GetIndex();
    if (item < 0) return;
    std::string name = std::string(m_varList->GetItemText(item).utf8_str());
    if (m_sessionConfig && m_sessionConfig->variables.count(name)) {
        m_sessionConfig->variables[name].enabled = m_varList->IsItemChecked(item);
        m_varList->SetItemTextColour(
            item, m_varList->IsItemChecked(item) ? *wxBLACK : wxColour(150, 150, 150));
    }
}

void SessionPanel::OnExecuteRead(wxCommandEvent&) {
    if (!m_txSession || !m_txSession->IsConnected()) return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;
    auto varCopy = m_sessionConfig->variables[m_selectedVarName];

    TransactionRequest req;
    req.slaveId = varCopy.params.value("slaveId", 1);
    if (varCopy.params.contains("functionCode")) {
        if (varCopy.params["functionCode"].is_string()) {
            req.functionCode = std::stoi(varCopy.params["functionCode"].get<std::string>());
        } else {
            req.functionCode = varCopy.params["functionCode"].get<int>();
        }
    }
    req.startAddress = varCopy.params.value("startAddress", 0);
    req.count = varCopy.params.value("count", 1);

    m_executeReadBtn->Enable(false);

    std::thread([this, req, name = m_selectedVarName]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name]() {
            m_executeReadBtn->Enable(true);
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

                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                    txt->ChangeValue(valStr);
                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                    txt->ChangeValue(rawStr);

                RefreshVariableList();
                AppendUnifiedLog(true, name, m_sessionConfig->variables[name].type, valStr, raw);
                double chartVal;
                if (TryParseNumeric(m_sessionConfig->variables[name].type, valStr, chartVal)) {
                    RecordData(name, chartVal);
                }
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

    m_executeWriteBtn->Enable(false);

    std::thread([this, req, name = m_selectedVarName, targetVal]() {
        auto resp = m_txSession->Execute(req);
        CallAfter([this, resp, name, targetVal]() {
            m_executeWriteBtn->Enable(true);
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

                if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                    txt->ChangeValue(rawStr);

                RefreshVariableList();
                AppendUnifiedLog(false, name, m_sessionConfig->variables[name].type, targetVal,
                                 raw);

                double chartVal;
                if (TryParseNumeric(m_sessionConfig->variables[name].type, targetVal, chartVal)) {
                    RecordData(name, chartVal);
                }
            } else {
                AppendUnifiedLog(false, name, "Error", resp.errorMessage, {});
            }
        });
    }).detach();
}

void SessionPanel::OnExecuteSend(wxCommandEvent&) {
    if ((!m_streamSession || !m_streamSession->IsConnected()) &&
        (!m_mqttSession || !m_mqttSession->IsConnected()))
        return;
    if (!m_sessionConfig || !m_sessionConfig->variables.count(m_selectedVarName)) return;

    auto& var = m_sessionConfig->variables[m_selectedVarName];
    std::string payload = var.currentValue;

    if (payload.empty()) {
        wxMessageBox(_("Payload is empty. Please fill in the 'Value' field."), _("Notice"),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    std::vector<uint8_t> bytes = EncodePayload(var.type, payload);

    bool success = false;
    if (m_mqttSession) {
        std::string topic = var.params.value("topic", "test/topic");
        int qos = var.params.value("qos", 0);
        m_mqttSession->Publish(topic, payload, qos);
        success = true;
    } else if (m_streamSession) {
        success = m_streamSession->Send(bytes);
    }

    if (success) {
        m_txBytes += bytes.size();
        if (m_txLabel)
            m_txLabel->SetLabel(
                wxString::Format(_("Sent: %llu bytes"), (unsigned long long)m_txBytes));

        wxString rawStr;
        for (uint8_t b : bytes) rawStr += wxString::Format("%02X ", b);
        var.rawBytes = std::string(rawStr.Trim().utf8_str());

        if (m_varOpControls.count("__raw_bytes")) {
            if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                txt->ChangeValue(rawStr.Trim());
        }

        AppendUnifiedLog(false, m_selectedVarName, var.type, payload, bytes);

        double chartVal;
        if (TryParseNumeric(var.type, payload, chartVal)) {
            RecordData(m_selectedVarName, chartVal);
        }
    } else {
        wxMessageBox(_("Send failed"), _("Error"), wxOK | wxICON_ERROR);
    }
}

void SessionPanel::OnAddVariable(wxCommandEvent&) {
    if (!m_sessionConfig) return;
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
        RefreshVariableList();
    }
}

void SessionPanel::OnCopyVariable(wxCommandEvent&) {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;
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
    RefreshVariableList();
}

void SessionPanel::OnEditVariable(wxCommandEvent&) {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;
    std::string key = std::string(m_varList->GetItemText(sel).utf8_str());
    auto it = m_sessionConfig->variables.find(key);
    if (it == m_sessionConfig->variables.end()) return;

    nlohmann::json editJson = {
        {"type", it->second.type}, {"access", it->second.access}, {"params", it->second.params}};
    VariableEditDialog dlg(this, m_protocol, key, editJson, true);

    if (dlg.ShowModal() == wxID_OK) {
        auto j = dlg.GetVarJson();
        it->second.type = j.value("type", "int32");
        it->second.access = j.value("access", "RW");
        if (j.contains("params")) it->second.params = j["params"];
        RefreshVariableList();
    }
}

void SessionPanel::OnDeleteVariable(wxCommandEvent&) {
    long sel = m_varList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel == -1) return;
    m_sessionConfig->variables.erase(std::string(m_varList->GetItemText(sel).utf8_str()));
    RefreshVariableList();
}

void SessionPanel::OnNotebookPageChanged(wxBookCtrlEvent& evt) {
    int sel = m_modeNotebook->GetSelection();
    if (m_sessionConfig && sel >= 0 && sel <= 2)
        m_sessionConfig->activeMode = static_cast<SessionMode>(sel);
    evt.Skip();
}

void SessionPanel::BuildTestFlowUi(wxPanel* parent, bool isAutoMode) {
    auto* main = new wxBoxSizer(wxVERTICAL);
    auto* stepList =
        new wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
    stepList->InsertColumn(0, _("Step Name"), wxLIST_FORMAT_LEFT, 250);
    stepList->InsertColumn(1, _("Status"), wxLIST_FORMAT_LEFT, 100);
    main->Add(stepList, 1, wxEXPAND | wxALL, 5);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* runBtn =
        new wxButton(parent, wxID_ANY, isAutoMode ? _("Run All") : _("Run (Step-by-Step)"));
    auto* stopBtn = new wxButton(parent, wxID_ANY, _("Stop"));
    auto* addBtn = new wxButton(parent, wxID_ANY, _("Add"));
    auto* copyBtn = new wxButton(parent, wxID_ANY, _("Copy"));
    auto* batchBtn = new wxButton(parent, wxID_ANY, _("Batch Create"));
    auto* editBtn = new wxButton(parent, wxID_ANY, _("Edit"));
    auto* delBtn = new wxButton(parent, wxID_ANY, _("Delete"));
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
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
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
        for (const auto& [n, v] : m_sessionConfig->variables) varNames.push_back(n);

        TestStepEditDialog dlg(this, initial, varNames);
        if (dlg.ShowModal() == wxID_OK) {
            nlohmann::json step = dlg.GetStepJson();
            long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
            int insertPos = (sel != -1) ? sel + 1 : flow.size();
            flow.insert(flow.begin() + insertPos, step);
            refreshList();
            int newIdx = (sel != -1) ? sel + 1 : stepList->GetItemCount() - 1;
            stepList->SetItemState(newIdx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            stepList->EnsureVisible(newIdx);
        }
    });

    copyBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        if (sel >= static_cast<long>(flow.size())) return;
        nlohmann::json copyStep = flow[sel];
        copyStep["name"] = copyStep.value("name", "Step") + " Copy";
        flow.insert(flow.begin() + sel + 1, copyStep);
        refreshList();
        stepList->SetItemState(sel + 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        stepList->EnsureVisible(sel + 1);
    });

    batchBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel == -1) {
            wxMessageBox(_("Please select a base step first."), _("Notice"), wxOK);
            return;
        }
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        if (sel >= static_cast<long>(flow.size())) return;

        wxDialog dlg(this, wxID_ANY, _("Batch Generate Steps"), wxDefaultPosition, wxSize(300, -1));
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        auto* grid = new wxFlexGridSizer(2, 8, 8);
        grid->AddGrowableCol(1, 1);
        grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Repeat Count:")), 0, wxALIGN_CENTER_VERTICAL);
        wxSpinCtrl* countCtrl = new wxSpinCtrl(&dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                               wxSP_ARROW_KEYS, 1, 10000, 10);
        grid->Add(countCtrl, 1, wxEXPAND);
        grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Interval (ms):")), 0,
                  wxALIGN_CENTER_VERTICAL);
        wxSpinCtrl* intervalCtrl = new wxSpinCtrl(&dlg, wxID_ANY, "", wxDefaultPosition,
                                                  wxDefaultSize, wxSP_ARROW_KEYS, 0, 600000, 1000);
        grid->Add(intervalCtrl, 1, wxEXPAND);
        grid->Add(new wxStaticText(&dlg, wxID_ANY, _("Auto Increment/Decrement:")), 0,
                  wxALIGN_CENTER_VERTICAL);
        wxSpinCtrl* incCtrl = new wxSpinCtrl(&dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                             wxSP_ARROW_KEYS, -1000000, 1000000, 1);
        grid->Add(incCtrl, 1, wxEXPAND);
        sizer->Add(grid, 1, wxEXPAND | wxALL, 15);
        sizer->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
        dlg.SetSizerAndFit(sizer);

        if (dlg.ShowModal() == wxID_OK) {
            int repeat = countCtrl->GetValue();
            int interval = intervalCtrl->GetValue();
            int autoInc = incCtrl->GetValue();
            nlohmann::json baseStep = flow[sel];
            std::string baseValue = baseStep.value("value", "");
            long long baseValInt = 0;
            bool isInt = false;
            if (baseStep["action"] == "write" || baseStep["action"] == "send") {
                try {
                    baseValInt = std::stoll(baseValue);
                    isInt = true;
                } catch (...) {
                }
            }
            for (int i = 1; i < repeat; ++i) {
                nlohmann::json subStep = baseStep;
                subStep["name"] =
                    baseStep.value("name", "Step") + " (" + std::to_string(i + 1) + ")";
                subStep["wait_ms"] = interval;
                if (isInt && autoInc != 0)
                    subStep["value"] = std::to_string(baseValInt + (i * autoInc));
                flow.insert(flow.begin() + sel + i, subStep);
            }
            refreshList();
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
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        std::vector<long> toDelete;
        long item = -1;
        while ((item = stepList->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1)
            toDelete.push_back(item);
        for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it)
            if (*it < static_cast<long>(flow.size())) flow.erase(flow.begin() + *it);
        refreshList();
    });

    upBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (sel <= 0) return;
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        std::swap(flow[sel], flow[sel - 1]);
        refreshList();
        stepList->SetItemState(sel - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
        stepList->EnsureVisible(sel - 1);
    });

    downBtn->Bind(wxEVT_BUTTON, [this, isAutoMode, stepList, refreshList](wxCommandEvent&) {
        if (!m_sessionConfig) return;
        long sel = stepList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        auto& flow = isAutoMode ? m_sessionConfig->autoMode.testFlow
                                : m_sessionConfig->interactiveMode.testFlow;
        if (sel == -1 || sel >= static_cast<long>(flow.size()) - 1) return;
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
            ITestStepPtr step;
            if (type == "delay")
                step = std::make_unique<DelayStep>(stepJson);
            else
                step = m_protocol->CreateTestStep(type, stepJson);

            if (step)
                steps.push_back(std::move(step));
            else {
                wxMessageBox(wxString::Format(_("Protocol '%s' does not support action '%s'."),
                                              wxString::FromUTF8(m_protocol->Id()),
                                              wxString::FromUTF8(type)),
                             _("Unsupported Action"), wxOK | wxICON_ERROR);
                return;
            }
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
                    RefreshVariableList();
                });
            });

        m_testRunner->OnStepLog([this](const std::string& msg) {
            CallAfter([this, msg]() {
                if (m_commonLogCtrl) {
                    m_commonLogCtrl->AppendText(wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                                                wxString::FromUTF8(msg) + "\n");
                    m_commonLogCtrl->SetInsertionPointEnd();
                }
            });
        });

        m_testRunner->OnDataRecorded(
            [this](const std::string& varName, double val) { RecordData(varName, val); });

        m_testRunner->OnFinished([this, runBtn, stopBtn](bool overallSuccess) {
            wxTheApp->CallAfter([this, runBtn, stopBtn, overallSuccess]() {
                runBtn->Enable(true);
                stopBtn->Enable(false);
                if (m_commonLogCtrl) {
                    m_commonLogCtrl->AppendText(
                        wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                        (overallSuccess ? _("Test Finished Successfully\n") : _("Test Failed\n")));
                    m_commonLogCtrl->SetInsertionPointEnd();
                }
                if (m_testRunner) {
                    delete m_testRunner;
                    m_testRunner = nullptr;
                }
            });
        });

        m_testRunner->OnWaitForNextStep([this, stepList](int nextIndex) {
            wxTheApp->CallAfter([this, stepList, nextIndex]() {
                if (nextIndex < stepList->GetItemCount()) {
                    stepList->SetItemState(nextIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                    stepList->EnsureVisible(nextIndex);
                }
                int ret = wxMessageBox(
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
    btnSizer->Add(copyBtn, 0, wxALL, 4);
    btnSizer->Add(batchBtn, 0, wxALL, 4);
    btnSizer->Add(editBtn, 0, wxALL, 4);
    btnSizer->Add(delBtn, 0, wxALL, 4);
    btnSizer->AddSpacer(10);
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
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__value"]))
                            txt->ChangeValue(wxString::FromUTF8(var.currentValue));
                    if (m_varOpControls.count("__raw_bytes"))
                        if (auto* txt = dynamic_cast<wxTextCtrl*>(m_varOpControls["__raw_bytes"]))
                            txt->ChangeValue(rawStr.Trim());
                    double chartVal;
                    if (TryParseNumeric(var.type, var.currentValue, chartVal))
                        RecordData(m_selectedVarName, chartVal);
                } else
                    parsedValue = "<Parse Failed>";
            } else
                parsedValue = "<No Variable Selected>";

            AppendUnifiedLog(true, varName, varType, parsedValue, copy, cid);
        });
    });

    m_streamSession->OnReconnectAttempt([this](int attempt) {
        CallAfter([this, attempt]() {
            if (m_commonLogCtrl) {
                m_commonLogCtrl->AppendText(
                    wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                    wxString::Format(_("Reconnecting (attempt %d) ...\n"), attempt));
                m_commonLogCtrl->SetInsertionPointEnd();
            }
        });
    });

    m_streamSession->OnClientConnected([this](int cid) {
        CallAfter([this, cid]() {
            ++m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            if (m_commonLogCtrl) {
                m_commonLogCtrl->AppendText(wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                                            wxString::Format(_("Client #%d connected\n"), cid));
                m_commonLogCtrl->SetInsertionPointEnd();
            }
        });
    });

    m_streamSession->OnClientDisconnected([this](int cid) {
        CallAfter([this, cid]() {
            if (m_clientCount > 0) --m_clientCount;
            UpdateClientsLabel();
            UpdateStateLabel(m_session->State());
            if (m_commonLogCtrl) {
                m_commonLogCtrl->AppendText(wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                                            wxString::Format(_("Client #%d disconnected\n"), cid));
                m_commonLogCtrl->SetInsertionPointEnd();
            }
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
        CallAfter([this, msg]() {
            if (m_commonLogCtrl) {
                m_commonLogCtrl->AppendText(wxDateTime::Now().Format("[%H:%M:%S.%l] ") +
                                            wxString::FromUTF8(msg) + "\n");
                m_commonLogCtrl->SetInsertionPointEnd();
            }
        });
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
                    double chartVal;
                    if (TryParseNumeric(var.type, var.currentValue, chartVal))
                        RecordData(m_selectedVarName, chartVal);
                }
            }
        });
    });
}

void SessionPanel::OnClear(wxCommandEvent&) {
    if (m_commonLogCtrl) m_commonLogCtrl->Clear();
}
void SessionPanel::UpdateClientsLabel() {
    if (m_clientsLabel) m_clientsLabel->SetLabel(wxString::Format(_("Clients: %d"), m_clientCount));
}

void SessionPanel::OnRecordSettings(wxCommandEvent&) {
    if (m_isRecording) {
        m_isRecording = false;
        m_recordBtn->SetLabel(_("Record Settings"));
        return;
    }
    if (!m_sessionConfig) return;

    wxArrayString choices;
    for (const auto& [name, var] : m_sessionConfig->variables)
        if (var.enabled) choices.Add(wxString::FromUTF8(name));
    if (choices.IsEmpty()) {
        wxMessageBox(_("No enabled variables to record."), _("Notice"), wxOK);
        return;
    }

    wxMultiChoiceDialog dlg(this, _("Select variables to record:"), _("Record Settings"), choices);
    wxArrayInt selections;
    for (unsigned int i = 0; i < choices.GetCount(); ++i) selections.Add(i);
    dlg.SetSelections(selections);

    if (dlg.ShowModal() == wxID_OK) {
        m_loggingVars.clear();
        for (int sel : dlg.GetSelections())
            m_loggingVars.insert(std::string(choices[sel].utf8_str()));
        m_isRecording = true;
        m_recordBtn->SetLabel(_("Stop Recording"));
        if (m_chartPanel) m_chartPanel->Clear();
    }
}

void SessionPanel::OnHistoryQuery(wxCommandEvent&) {
    if (!m_sessionConfig) return;
    std::vector<std::string> varNames;
    for (const auto& [n, v] : m_sessionConfig->variables)
        if (v.access != "WO") varNames.push_back(n);
    if (varNames.empty()) {
        wxMessageBox(_("No readable variables found."), _("Notice"), wxOK);
        return;
    }

    HistoryDialog dlg(this, &m_db, std::string(m_profilePath.utf8_str()), varNames);
    dlg.ShowModal();
}

}  // namespace icu
