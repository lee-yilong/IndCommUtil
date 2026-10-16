#include "app/MainFrame.h"

#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/simplebook.h>
#include <wx/tokenzr.h>

#include <optional>
#include <vector>

#include "app/App.h"
#include "config/ConnectionProfile.h"
#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "core/Session.h"
#include "log/Log.h"
#include "protocols/codec/RawCodec.h"
#include "protocols/tcp/TcpTransport.h"
#include "settings/UISettings.h"
#include "spdlog/spdlog.h"
#include "ui/AppTaskBarIcon.h"
#include "ui/GenericProtocolDialog.h"
#include "ui/LogFrame.h"
#include "ui/ProtocolSessionPanel.h"

namespace icu {

// clang-format off
// wxWidgets 事件表宏展开后是一条语句，clang-format 会把后续行的缩进逐级累积
wxBEGIN_EVENT_TABLE(MainFrame, wxFrame) EVT_CLOSE(MainFrame::OnClose)
    EVT_MENU(idMenuQuit, MainFrame::OnQuit)
    EVT_MENU(idMenuAbout, MainFrame::OnAbout)
    EVT_MENU(idMenuLog, MainFrame::OnShowLog)
    EVT_MENU(idMenuOpenConnection, MainFrame::OnOpenConnection)
    EVT_MENU(idMenuSaveConnection, MainFrame::OnSaveConnection)
    EVT_MENU(idMenuSaveConnectionAs, MainFrame::OnSaveConnectionAs)
    EVT_MENU(idMenuCloseConnection, MainFrame::OnCloseConnection)
    EVT_MENU_RANGE(idMenuNewProtocolBase, idMenuNewProtocolEnd, MainFrame::OnMenuOpenProtocol)
    EVT_MENU(idMenuLangZhCN, MainFrame::OnLangZhCN)
    EVT_MENU(idMenuLangEnUS, MainFrame::OnLangEnUS)
    EVT_MENU_RANGE(idMenuRecentBase, idMenuRecentBase + 100, MainFrame::OnRecentConnection)
wxEND_EVENT_TABLE();  // clang-format on

MainFrame::MainFrame(wxFrame* parent, const wxString& title) : wxFrame(parent, wxID_ANY, title) {
    BuildMenuBar();
    BuildLayout();

    log::Init();
    UpdateConnectionUI();
    LoadRecentConnections();

    const wxSize kDefaultSize = FromDIP(wxSize(1200, 800));
    const wxSize kMinSize = FromDIP(wxSize(800, 600));

    bool restored = false;
    const auto geo = settings::UISettings::Instance().LoadMainWindowGeometry();
    if (geo.valid && wxDisplay().GetClientArea().Intersects(geo.rect)) {
        SetSize(geo.rect);
        if (geo.maximized) Maximize(true);
        restored = true;
    }
    if (!restored) {
        SetClientSize(kDefaultSize);
        Centre();
    }

    SetMinClientSize(kMinSize);

    m_trayIcon = new AppTaskBarIcon();
    m_trayIcon->OnShow([this]() { RestoreFromTray(); });
    m_trayIcon->OnExit([this]() {
        m_forceExit = true;
        CallAfter([this]() { Close(true); });
    });

    SPDLOG_INFO(_("Industrial Communication Utility started"));
}

MainFrame::~MainFrame() {
    m_sessionMgr.Close();
    SPDLOG_INFO(_("MainFrame destroyed"));
}

void MainFrame::BuildMenuBar() {
    auto* menuBar = new wxMenuBar();

    m_fileMenu = new wxMenu(_T(""));

    auto* newConnMenu = new wxMenu(_T(""));
    m_protocolMenuMap.clear();

    int nextId = idMenuNewProtocolBase;
    for (auto* proto : ProtocolRegistry::Instance().All()) {
        if (nextId >= idMenuNewProtocolEnd) {
            SPDLOG_WARN(_("Too many protocols, menu ID overflow"));
            break;
        }
        const wxString label = wxString::FromUTF8(proto->DisplayName());
        newConnMenu->Append(nextId, label);

        m_protocolMenuMap[nextId] = proto;
        ++nextId;
    }

    m_fileMenu->AppendSubMenu(newConnMenu, _("&New Connection"), _("Create a new connection"));

    m_recentMenu = new wxMenu(_T(""));
    m_recentMenuItem = m_fileMenu->AppendSubMenu(m_recentMenu, _("Recent &Connections"),
                                                 _("Recently used connections"));

    m_fileMenu->Append(idMenuOpenConnection, _("&Open Connection\tCtrl+O"),
                       _("Open a saved connection"));

    m_fileMenu->Append(idMenuSaveConnection, _("&Save Connection\tCtrl+S"),
                       _("Save the current connection profile"));

    m_fileMenu->Append(idMenuSaveConnectionAs, _("Save Connection &As...\tCtrl+Shift+S"),
                       _("Save the current connection profile to a new file"));

    m_fileMenu->Append(idMenuCloseConnection, _("&Close Connection\tCtrl+W"),
                       _("Close the current connection"));

    m_fileMenu->AppendSeparator();
    m_fileMenu->Append(idMenuQuit, _("&Quit\tAlt+F4"), _("Quit the application"));

    menuBar->Append(m_fileMenu, _("&File"));

    auto* viewMenu = new wxMenu(_T(""));
    viewMenu->Append(idMenuLog, _("&Show Log\tF2"), _("Show application logs"));
    menuBar->Append(viewMenu, _("&View"));

    auto* settingsMenu = new wxMenu(_T(""));
    auto* langMenu = new wxMenu(_T(""));
    langMenu->AppendRadioItem(idMenuLangZhCN, _("Simplified Chinese"));
    langMenu->AppendRadioItem(idMenuLangEnUS, _("English"));
    auto* app = static_cast<icu::App*>(wxTheApp);
    const wxString cur = app ? app->GetCurrentLanguage() : "zh_CN";
    if (cur.StartsWith("en"))
        langMenu->Check(idMenuLangEnUS, true);
    else
        langMenu->Check(idMenuLangZhCN, true);
    settingsMenu->AppendSubMenu(langMenu, _("Language"));
    menuBar->Append(settingsMenu, _("&Settings"));

    auto* helpMenu = new wxMenu(_T(""));
    helpMenu->Append(idMenuAbout, _("&About\tF1"), _("Show info about this application"));
    menuBar->Append(helpMenu, _("&Help"));

    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText(_("Ready"));
}

void MainFrame::BuildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    m_contentBook = new wxSimplebook(this, wxID_ANY);

    auto* welcome = new wxPanel(m_contentBook);
    {
        auto* s = new wxBoxSizer(wxVERTICAL);
        auto* txt =
            new wxStaticText(welcome, wxID_ANY,
                             _("No active connection.\nUse File → New Connection to get started."));
        txt->SetForegroundColour(wxColour(128, 128, 128));
        s->AddStretchSpacer();
        s->Add(txt, 0, wxALIGN_CENTER);
        s->AddStretchSpacer();
        welcome->SetSizer(s);
    }
    m_contentBook->AddPage(welcome, _("Welcome"));

    m_contentBook->AddPage(new wxPanel(m_contentBook), _("Session"));
    m_contentBook->SetSelection(0);

    root->Add(m_contentBook, 1, wxEXPAND);

    SetSizer(root);
    root->Layout();
}

void MainFrame::ShowWelcomePage() {
    m_sessionPanel = nullptr;
    while (m_contentBook->GetPageCount() > 1) m_contentBook->DeletePage(1);
    m_contentBook->AddPage(new wxPanel(m_contentBook), _("Session"));
    m_contentBook->SetSelection(0);
}

void MainFrame::ShowSessionPage(Session* session) {
    m_sessionPanel = nullptr;
    while (m_contentBook->GetPageCount() > 1) m_contentBook->DeletePage(1);

    m_sessionPanel = new ProtocolSessionPanel(m_contentBook, session);
    m_contentBook->AddPage(m_sessionPanel, _("Session"));
    m_contentBook->SetSelection(1);
}

void MainFrame::OnClose(wxCloseEvent& evt) {
    if (m_forceExit) {
        SaveRecentConnections();
        settings::UISettings::Instance().SaveMainWindowGeometry(GetScreenRect(), IsMaximized());

        SPDLOG_INFO(_("Application exiting"));
        m_sessionMgr.Close();

        if (m_logFrame) {
            m_logFrame->ForceDestroy();
            m_logFrame = nullptr;
        }

        if (m_trayIcon) {
            m_trayIcon->RemoveIcon();
            wxPendingDelete.Append(m_trayIcon);
            m_trayIcon = nullptr;
        }

        Destroy();

        if (wxTheApp) wxTheApp->ExitMainLoop();
        return;
    }

    if (m_activeProfile && IsModified()) {
        const int ret = wxMessageBox(
            wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName()),
            _("Save Connection Profile"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

        if (ret == wxCANCEL) {
            evt.Veto();
            SPDLOG_INFO(_("Close cancelled by user"));
            return;
        }
        if (ret == wxYES) {
            SaveCurrentProfile();
            if (IsModified()) {
                evt.Veto();
                SPDLOG_INFO(_("Save cancelled, close aborted"));
                return;
            }
        }
    }

    const int ret = wxMessageBox(_("Minimize to system tray instead of exiting?"), _("Close"),
                                 wxYES_NO | wxICON_QUESTION, this);

    if (ret == wxYES) {
        evt.Veto();
        MinimizeToTray();
        return;
    }

    SaveRecentConnections();
    settings::UISettings::Instance().SaveMainWindowGeometry(GetScreenRect(), IsMaximized());

    SPDLOG_INFO(_("Application exiting"));
    m_sessionMgr.Close();

    if (m_logFrame) {
        m_logFrame->ForceDestroy();
        m_logFrame = nullptr;
    }
    if (m_trayIcon) {
        m_trayIcon->RemoveIcon();
        auto* icon = m_trayIcon;
        m_trayIcon = nullptr;
        wxTheApp->CallAfter([icon]() { delete icon; });
    }

    Destroy();
    if (wxTheApp) wxTheApp->ExitMainLoop();
}

void MainFrame::OnQuit(wxCommandEvent&) {
    m_forceExit = true;
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox(_("Industrial Communication Utility\nA tool for testing industrial protocols."),
                 _("About"), wxOK | wxICON_INFORMATION, this);
    SPDLOG_INFO(_("About dialog shown"));
}

void MainFrame::OnShowLog(wxCommandEvent&) {
    if (!m_logFrame) {
        m_logFrame = new LogFrame(this);
    }
    m_logFrame->Show();
    m_logFrame->Raise();
}

void MainFrame::OnOpenConnection(wxCommandEvent&) {
    SPDLOG_INFO(_("User invoked Open Connection"));

    if (m_sessionMgr.Active()) {
        wxMessageBox(_("Please close the current connection first."), _("Notice"),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    wxFileDialog dlg(this, _("Open Connection Configuration"), "", "",
                     "Connection files (*.json)|*.json|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) {
        SPDLOG_INFO(_("Open connection cancelled by user"));
        return;
    }

    const wxString path = dlg.GetPath();

    ActiveProfile profile;
    if (!ConnectionProfile::Load(path, profile)) {
        wxMessageBox(wxString::Format(_("Failed to load connection profile:\n%s"), path),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    m_activeProfile = profile;
    m_savedProfile = profile;
    m_lastDialogProfile = profile;
    m_currentProfilePath = path;

    AddRecentConnection(path);

    ICU_SPDLOG_INFO(_("Opening connection from file: %s"), path);

    CreateAndOpenSession(profile);
    UpdateTitle();
}

void MainFrame::OnCloseConnection(wxCommandEvent&) {
    if (!m_sessionMgr.Active()) return;

    ICU_SPDLOG_INFO(_("Closing connection: %s"),
                    wxString::FromUTF8(m_sessionMgr.Active()->Target().c_str()));

    if (m_activeProfile && IsModified()) {
        const int ret = wxMessageBox(
            wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName()),
            _("Save Connection Profile"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

        if (ret == wxCANCEL) {
            SPDLOG_INFO(_("Close cancelled by user"));
            return;
        }
        if (ret == wxYES) {
            SaveCurrentProfile();
            if (IsModified()) {
                SPDLOG_INFO(_("Save cancelled, close aborted"));
                return;
            }
        }
    }

    m_sessionMgr.Close();
    m_activeProfile.reset();
    m_savedProfile.reset();
    m_currentProfilePath.Clear();
    ShowWelcomePage();
    UpdateConnectionUI();
    UpdateTitle();
    SetStatusText(_("Disconnected"));
}

void MainFrame::OnRecentConnection(wxCommandEvent& evt) {
    size_t index = static_cast<size_t>(evt.GetId() - idMenuRecentBase);
    if (index >= m_recentConnections.GetCount()) return;

    if (m_sessionMgr.IsConnected()) {
        wxMessageBox(_("Please close the current connection first."), _("Notice"),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    const wxString path = m_recentConnections[index];

    if (!wxFileName::FileExists(path)) {
        wxMessageBox(wxString::Format(_("Connection profile not found:\n%s"), path), _("Error"),
                     wxOK | wxICON_ERROR, this);
        m_recentConnections.RemoveAt(index);
        RebuildRecentMenu();
        SaveRecentConnections();
        return;
    }

    ActiveProfile profile;
    if (!ConnectionProfile::Load(path, profile)) {
        wxMessageBox(wxString::Format(_("Failed to load connection profile:\n%s"), path),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    m_activeProfile = profile;
    m_savedProfile = profile;
    m_lastDialogProfile = profile;
    m_currentProfilePath = path;

    AddRecentConnection(path);
    CreateAndOpenSession(profile);
    UpdateTitle();
}

void MainFrame::UpdateConnectionUI() {
    const bool hasSession = (m_sessionMgr.Active() != nullptr);

    if (m_fileMenu) {
        m_fileMenu->Enable(idMenuSaveConnection, m_activeProfile.has_value());
        m_fileMenu->Enable(idMenuSaveConnectionAs, m_activeProfile.has_value());
        m_fileMenu->Enable(idMenuCloseConnection, hasSession);
    }
    if (m_recentMenuItem) m_recentMenuItem->Enable(!m_recentConnections.IsEmpty());
}

void MainFrame::AddRecentConnection(const wxString& path) {
    wxFileName fn(path);
    if (fn.GetExt().Lower() != "json") {
        ICU_SPDLOG_WARN(_("AddRecentConnection: not a .json path, ignored: %s"), path);
        return;
    }

    int idx = m_recentConnections.Index(path);
    if (idx != wxNOT_FOUND) m_recentConnections.RemoveAt(idx);

    m_recentConnections.Insert(path, 0);
    while (m_recentConnections.GetCount() > MAX_RECENT)
        m_recentConnections.RemoveAt(m_recentConnections.GetCount() - 1);

    RebuildRecentMenu();
    SaveRecentConnections();
}

void MainFrame::RebuildRecentMenu() {
    if (!m_recentMenu) return;

    while (m_recentMenu->GetMenuItemCount() > 0)
        m_recentMenu->Delete(m_recentMenu->FindItemByPosition(0));

    if (m_recentConnections.IsEmpty()) {
        m_recentMenu->Append(idMenuRecentBase, _("(None)"));
        m_recentMenu->Enable(idMenuRecentBase, false);
    } else {
        for (size_t i = 0; i < m_recentConnections.GetCount(); ++i) {
            wxFileName fn(m_recentConnections[i]);
            wxString label = wxString::Format(_("&%d %s"), (int)(i + 1), fn.GetName());
            wxMenuItem* item = m_recentMenu->Append(idMenuRecentBase + i, label);
            item->SetHelp(m_recentConnections[i]);
        }
    }
    if (m_recentMenuItem) m_recentMenuItem->Enable(!m_recentConnections.IsEmpty());
}

void MainFrame::OnLangZhCN(wxCommandEvent&) {
    SPDLOG_INFO(_("User selected Simplified Chinese"));
    auto* app = static_cast<icu::App*>(wxTheApp);
    if (app)
        app->SwitchLanguage("zh_CN");
    else
        SPDLOG_ERROR(_("wxTheApp is not icu::App"));
}

void MainFrame::OnLangEnUS(wxCommandEvent&) {
    SPDLOG_INFO(_("User selected English"));
    auto* app = static_cast<icu::App*>(wxTheApp);
    if (app)
        app->SwitchLanguage("en_US");
    else
        SPDLOG_ERROR(_("wxTheApp is not icu::App"));
}

void MainFrame::LoadRecentConnections() {
    const wxString packed = settings::UISettings::Instance().GetRecentConnectionsPacked();

    m_recentConnections.Clear();
    if (!packed.IsEmpty()) {
        wxStringTokenizer tok(packed, "\n");
        while (tok.HasMoreTokens()) {
            const wxString path = tok.GetNextToken();
            if (!path.IsEmpty()) m_recentConnections.Add(path);
        }
    }

    for (int i = (int)m_recentConnections.GetCount() - 1; i >= 0; --i) {
        if (!wxFileName::FileExists(m_recentConnections[i])) m_recentConnections.RemoveAt(i);
    }

    RebuildRecentMenu();
    SaveRecentConnections();
}

void MainFrame::SaveRecentConnections() {
    wxString packed;
    for (size_t i = 0; i < m_recentConnections.GetCount(); ++i) {
        if (i > 0) packed += "\n";
        packed += m_recentConnections[i];
    }

    settings::UISettings::Instance().SetRecentConnectionsPacked(packed);
}

bool MainFrame::IsModified() const {
    if (!m_activeProfile) return false;
    if (!m_savedProfile) return true;
    return *m_activeProfile != *m_savedProfile;
}

wxString MainFrame::GetDisplayName() const {
    if (m_currentProfilePath.IsEmpty()) return _("Untitled");
    return wxFileName(m_currentProfilePath).GetName();
}

void MainFrame::UpdateTitle() {
    wxString title = _("Industrial Communication Utility");
    if (m_activeProfile) {
        title += " - " + GetDisplayName();
        if (IsModified()) title += " *";
    }
    SetTitle(title);
}

void MainFrame::SaveCurrentProfile() {
    if (!m_activeProfile) return;

    if (m_currentProfilePath.IsEmpty()) {
        SaveCurrentProfileAs();
        return;
    }

    auto* proto = GetActiveProtocol();
    if (!proto) return;

    if (ConnectionProfile::Save(m_currentProfilePath, m_activeProfile->protocolId,
                                m_activeProfile->modeIndex, proto, m_activeProfile->fields)) {
        m_savedProfile = m_activeProfile;
        m_lastDialogProfile = m_activeProfile;
        AddRecentConnection(m_currentProfilePath);
        UpdateTitle();
        ICU_SPDLOG_INFO(_("Profile saved: %s"), m_currentProfilePath);
    } else {
        wxMessageBox(_("Failed to save connection profile."), _("Error"), wxOK | wxICON_ERROR,
                     this);
    }
}

void MainFrame::SaveCurrentProfileAs() {
    if (!m_activeProfile) return;

    auto* proto = GetActiveProtocol();
    if (!proto) return;

    const wxString defaultName =
        m_currentProfilePath.IsEmpty()
            ? ConnectionProfile::MakeDefaultFileName(proto, m_activeProfile->modeIndex,
                                                     m_activeProfile->fields)
            : wxFileName(m_currentProfilePath).GetFullName();

    wxFileDialog dlg(this, _("Save Connection Profile"), "", defaultName,
                     "Connection files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    wxString path = dlg.GetPath();
    if (path.IsEmpty()) return;

    // 扩展名兜底（在 ShowModal 之后，不影响对话框）
    wxFileName fn(path);
    if (fn.GetExt().Lower() != "json") {
        fn.SetExt("json");
        path = fn.GetFullPath();
    }

    if (ConnectionProfile::Save(path, m_activeProfile->protocolId, m_activeProfile->modeIndex,
                                proto, m_activeProfile->fields)) {
        m_currentProfilePath = path;
        m_savedProfile = m_activeProfile;
        m_lastDialogProfile = m_activeProfile;
        AddRecentConnection(path);
        UpdateTitle();
        ICU_SPDLOG_INFO(_("Profile saved as: %s"), path);
    } else {
        wxMessageBox(_("Failed to save connection profile."), _("Error"), wxOK | wxICON_ERROR,
                     this);
    }
}

void MainFrame::OnSaveConnection(wxCommandEvent&) {
    SPDLOG_INFO(_("User clicked Save Connection"));
    SaveCurrentProfile();
}

void MainFrame::OnSaveConnectionAs(wxCommandEvent&) {
    SPDLOG_INFO(_("User clicked Save Connection As"));
    SaveCurrentProfileAs();
}

void MainFrame::MinimizeToTray() {
    Hide();
    SPDLOG_INFO(_("Minimized to system tray"));
}

void MainFrame::RestoreFromTray() {
    Show();
    Raise();
    Iconize(false);
    SPDLOG_INFO(_("Restored from system tray"));
}

void MainFrame::OnMenuOpenProtocol(wxCommandEvent& evt) {
    const int id = evt.GetId();
    auto it = m_protocolMenuMap.find(id);
    if (it == m_protocolMenuMap.end()) {
        ICU_SPDLOG_WARN(_("Unknown protocol menu id: %d"), id);
        return;
    }

    IProtocol* proto = it->second;
    ICU_SPDLOG_INFO(_("User invoked New Connection for protocol: %s"),
                    wxString::FromUTF8(proto->Id().c_str()));

    if (m_sessionMgr.Active()) {
        wxMessageBox(_("Please close the current connection first."), _("Notice"),
                     wxOK | wxICON_WARNING, this);
        return;
    }

    std::map<std::string, FieldValue> initial;
    int initialMode = 0;
    if (m_lastDialogProfile && m_lastDialogProfile->protocolId == proto->Id()) {
        initial = m_lastDialogProfile->fields;
        initialMode = m_lastDialogProfile->modeIndex;
    }

    GenericProtocolDialog dlg(this, proto, initialMode, initial);
    if (dlg.ShowModal() != wxID_OK) return;

    ActiveProfile profile;
    profile.protocolId = proto->Id();
    profile.modeIndex = dlg.GetModeIndex();
    profile.fields = dlg.GetValues();

    m_lastDialogProfile = profile;
    m_activeProfile = profile;
    m_savedProfile.reset();
    m_currentProfilePath.Clear();

    CreateAndOpenSession(profile);
    UpdateTitle();
}

IProtocol* MainFrame::GetActiveProtocol() const {
    if (!m_activeProfile) return nullptr;
    return ProtocolRegistry::Instance().Find(m_activeProfile->protocolId);
}

void MainFrame::CreateAndOpenSession(const ActiveProfile& profile) {
    auto* proto = ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) return;

    SessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;

    Session* rawPtr = session.get();
    const std::string target = rawPtr->Target();

    m_sessionMgr.SetActive(std::move(session));

    ShowSessionPage(rawPtr);
    AttachSessionCallbacks(rawPtr);

    if (!rawPtr->Open()) {
        SPDLOG_ERROR(_("Failed to open session"));
    }

    ICU_SPDLOG_INFO(_("Creating session: %s"), wxString::FromUTF8(target.c_str()));
    SetStatusText(wxString::Format(_("Connecting to %s ..."), wxString::FromUTF8(target.c_str())));
    UpdateConnectionUI();
}

void MainFrame::AttachSessionCallbacks(Session* rawPtr) {
    rawPtr->OnStateChanged([this](SessionState st) {
        CallAfter([this, st]() {
            if (!m_sessionMgr.Active()) return;
            UpdateConnectionUI();
            if (m_sessionPanel) m_sessionPanel->RefreshFromSession();

            if (auto* s = m_sessionMgr.Active()) {
                const wxString target = wxString::FromUTF8(s->Target().c_str());
                const bool isServer = (s->IsServerMode());

                switch (st) {
                    case SessionState::Connected:
                        if (isServer)
                            SetStatusText(wxString::Format(_("Listening on %s"), target));
                        else
                            SetStatusText(wxString::Format(
                                _("Connected [%s]: %s"), wxString::FromUTF8(s->Protocol().c_str()),
                                target));
                        break;
                    case SessionState::Connecting:
                        SetStatusText(wxString::Format(_("Connecting to %s ..."), target));
                        break;
                    case SessionState::Reconnecting:
                        SetStatusText(wxString::Format(_("Reconnecting to %s ..."), target));
                        break;
                    default:
                        SetStatusText(_("Disconnected"));
                        break;
                }
            }
        });
    });

    rawPtr->OnMessage([this](int clientId, const std::vector<uint8_t>& msg) {
        auto copy = msg;
        CallAfter([this, clientId, copy = std::move(copy)]() {
            if (!m_sessionMgr.Active()) return;
            if (m_sessionPanel) m_sessionPanel->OnMessage(clientId, copy);
        });
    });

    rawPtr->OnReconnectAttempt([this](int attempt) {
        CallAfter([this, attempt]() {
            if (!m_sessionMgr.Active()) return;
            if (m_sessionPanel) m_sessionPanel->OnReconnectAttempt(attempt);
        });
    });

    rawPtr->OnClientConnected([this](int clientId) {
        CallAfter([this, clientId]() {
            if (!m_sessionMgr.Active()) return;
            if (m_sessionPanel) m_sessionPanel->OnClientConnected(clientId);
        });
    });
    rawPtr->OnClientDisconnected([this](int clientId) {
        CallAfter([this, clientId]() {
            if (!m_sessionMgr.Active()) return;
            if (m_sessionPanel) m_sessionPanel->OnClientDisconnected(clientId);
        });
    });

    if (m_sessionPanel) {
        m_sessionPanel->OnRequestOpen([this]() {
            SPDLOG_INFO(_("User clicked Open Connection"));

            if (!m_activeProfile) return;

            CallAfter([this]() {
                m_sessionMgr.Close();
                ShowWelcomePage();
                UpdateConnectionUI();

                CreateAndOpenSession(*m_activeProfile);
                UpdateTitle();
            });
        });

        m_sessionPanel->OnRequestConfigure([this]() {
            SPDLOG_INFO(_("User clicked Configure Connection"));

            auto* proto = GetActiveProtocol();
            if (!proto || !m_activeProfile) return;

            if (m_sessionMgr.IsConnected()) {
                wxMessageBox(_("Please close the current connection first."), _("Notice"),
                             wxOK | wxICON_WARNING, this);
                return;
            }

            GenericProtocolDialog dlg(this, proto, m_activeProfile->modeIndex,
                                      m_activeProfile->fields);
            if (dlg.ShowModal() != wxID_OK) return;

            ActiveProfile p;
            p.protocolId = proto->Id();
            p.modeIndex = dlg.GetModeIndex();
            p.fields = dlg.GetValues();

            m_activeProfile = p;
            m_lastDialogProfile = p;

            UpdateTitle();
            ICU_SPDLOG_INFO(_("Configuration updated (modified=%s)"),
                            IsModified() ? _("yes") : _("no"));
        });
    }
}

}  // namespace icu
