#include "app/MainFrame.h"

#include <wx/aui/auibook.h>
#include <wx/datetime.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/icon.h>
#include <wx/msw/wrapwin.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>

#include <nlohmann/json.hpp>

#include "app/App.h"
#include "config/ConnectionProfile.h"
#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "core/StreamSession.h"
#include "log/Log.h"
#include "protocols/codec/RawCodec.h"
#include "settings/UISettings.h"
#include "spdlog/spdlog.h"
#include "ui/AppTaskBarIcon.h"
#include "ui/GenericProtocolDialog.h"
#include "ui/LogFrame.h"
#include "ui/SessionPanel.h"
#include "ui/SettingsDialog.h"

namespace icu {

// clang-format off
wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_CLOSE(MainFrame::OnClose)
    EVT_MENU(idMenuQuit, MainFrame::OnQuit)
    EVT_MENU(idMenuAbout, MainFrame::OnAbout)
    EVT_MENU(idMenuLog, MainFrame::OnShowLog)
    EVT_MENU(idMenuSettingsOptions, MainFrame::OnShowSettings)
    EVT_MENU(idMenuOpenConnection, MainFrame::OnOpenConnection)
    EVT_MENU(idMenuSaveConnection, MainFrame::OnSaveConnection)
    EVT_MENU(idMenuSaveConnectionAs, MainFrame::OnSaveConnectionAs)
    EVT_MENU(idMenuCloseConnection, MainFrame::OnCloseConnection)
    EVT_MENU_RANGE(idMenuNewProtocolBase, idMenuNewProtocolEnd, MainFrame::OnMenuOpenProtocol)
    EVT_MENU(idMenuLangZhCN, MainFrame::OnLangZhCN)
    EVT_MENU(idMenuLangEnUS, MainFrame::OnLangEnUS)
    EVT_MENU_RANGE(idMenuRecentBase, idMenuRecentBase + 100, MainFrame::OnRecentConnection)

    EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, MainFrame::OnNotebookPageChanged)
    EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, MainFrame::OnNotebookPageClose)
wxEND_EVENT_TABLE();
// clang-format on

MainFrame::MainFrame(wxFrame* parent, const wxString& title) : wxFrame(parent, wxID_ANY, title) {
#ifdef _WIN32
    HICON hIcon = static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                  IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    if (hIcon) {
        wxIcon icon;
        icon.CreateFromHICON(hIcon);
        SetIcon(icon);
    }
#endif
    BuildMenuBar();
    BuildLayout();

    UpdateUIState();
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

    SPDLOG_INFO(_("IndComm started"));
}

MainFrame::~MainFrame() {
    for (auto& tab : m_tabs) {
        if (tab->session) {
            tab->session->DetachAllCallbacks();
            tab->session->Close();
        }
    }
    m_tabs.clear();
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

    settingsMenu->AppendSeparator();
    settingsMenu->Append(idMenuSettingsOptions, _("&Options..."),
                         _("Configure application settings"));

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

    // ★ 核心修改：将 wxNotebook 替换为 wxAuiNotebook，并开启关闭按钮
    m_notebook = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE |
                                       wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_CLOSE_ON_ALL_TABS);

    root->Add(m_notebook, 1, wxEXPAND);

    SetSizer(root);
    root->Layout();
}

TabContext* MainFrame::GetActiveTab() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        return m_tabs[m_activeTabIndex].get();
    }
    return nullptr;
}

void MainFrame::AddTab(IProtocol* proto, int modeIndex, ISessionPtr session,
                       const ActiveProfile& profile, const wxString& path) {
    auto tab = std::make_unique<TabContext>();
    tab->protocol = proto;
    tab->modeIndex = modeIndex;
    tab->session = std::move(session);
    tab->activeProfile = profile;
    tab->savedProfile = profile;
    tab->profilePath = path;

    ISession* rawPtr = tab->session.get();

    tab->panel = new SessionPanel(m_notebook, proto, modeIndex, rawPtr,
                                  &(tab->activeProfile->session), path);

    int index = m_tabs.size();
    m_tabs.push_back(std::move(tab));

    m_notebook->AddPage(m_tabs[index]->panel, GetDisplayName(index), true);
    m_activeTabIndex = index;

    auto* panel = m_tabs[index]->panel;

    panel->OnRequestConfigure([this, index]() {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        auto* t = m_tabs[index].get();
        if (!t->protocol || !t->activeProfile) return;

        GenericProtocolDialog dlg(this, t->protocol, t->activeProfile->modeIndex,
                                  t->activeProfile->fields);
        if (dlg.ShowModal() != wxID_OK) return;

        ActiveProfile newP;
        newP.protocolId = t->protocol->Id();
        newP.modeIndex = dlg.GetModeIndex();
        newP.fields = dlg.GetValues();

        ISessionPtr newSession = t->protocol->CreateSession(newP.modeIndex, newP.fields);
        if (!newSession) return;

        wxString pathCopy = t->profilePath;
        std::optional<ActiveProfile> savedCopy = t->savedProfile;
        IProtocol* protoCopy = t->protocol;

        RemoveTab(index);

        AddTab(protoCopy, newP.modeIndex, std::move(newSession), newP, pathCopy);

        auto& newTab = m_tabs.back();
        newTab->savedProfile = savedCopy;
        UpdateTabTitle(m_tabs.size() - 1);
    });

    panel->OnStateUpdated([this, index](SessionState st) {
        if (index == m_activeTabIndex) {
            UpdateUIState();
        }
    });

    UpdateUIState();

    if (!rawPtr->Open()) {
        SPDLOG_ERROR(_("Failed to open session"));
    }
}

void MainFrame::RemoveTab(int index, bool skipNotebookDelete) {
    if (index < 0 || index >= (int)m_tabs.size()) return;

    auto& tab = m_tabs[index];
    if (tab->session) {
        tab->session->DetachAllCallbacks();
        tab->session->Close();
    }

    if (!skipNotebookDelete) {
        m_notebook->DeletePage(index);
    }
    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
    } else {
        m_activeTabIndex = m_notebook->GetSelection();
    }
    UpdateUIState();
}

void MainFrame::OnNotebookPageChanged(wxAuiNotebookEvent& evt) {
    m_activeTabIndex = evt.GetSelection();
    UpdateUIState();
    evt.Skip();
}

void MainFrame::OnNotebookPageClose(wxAuiNotebookEvent& evt) {
    int index = evt.GetSelection();
    if (index < 0 || index >= (int)m_tabs.size()) return;

    auto& tab = m_tabs[index];
    if (tab->activeProfile && IsModified(index)) {
        const int ret = wxMessageBox(
            wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName(index)),
            _("Save Connection Profile"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

        if (ret == wxCANCEL) {
            evt.Veto();
            return;
        }
        if (ret == wxYES) {
            SaveCurrentProfile(false, index);
            if (IsModified(index)) {
                evt.Veto();
                return;
            }
        }
    }

    RemoveTab(index, true);
}

void MainFrame::UpdateUIState() {
    bool hasTab = (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size());

    if (m_fileMenu) {
        m_fileMenu->Enable(idMenuSaveConnection, hasTab);
        m_fileMenu->Enable(idMenuSaveConnectionAs, hasTab);
        m_fileMenu->Enable(idMenuCloseConnection, hasTab);
    }
    if (m_recentMenuItem) m_recentMenuItem->Enable(!m_recentConnections.IsEmpty());

    if (!hasTab) {
        SetTitle(_("IndComm"));
        SetStatusText(_("Ready"));
    } else {
        UpdateTabTitle(m_activeTabIndex);
        auto* tab = GetActiveTab();
        if (tab && tab->session) {
            const wxString targetStr = wxString::FromUTF8(tab->session->Target().c_str());
            SessionState st = tab->session->State();
            switch (st) {
                case SessionState::Connected:
                    if (tab->session->IsServerMode())
                        SetStatusText(wxString::Format(_("Listening on %s"), targetStr));
                    else
                        SetStatusText(wxString::Format(
                            _("Connected [%s]: %s"),
                            wxString::FromUTF8(tab->session->TransportName().c_str()), targetStr));
                    break;
                case SessionState::Connecting:
                    SetStatusText(wxString::Format(_("Connecting to %s ..."), targetStr));
                    break;
                case SessionState::Reconnecting:
                    SetStatusText(wxString::Format(_("Reconnecting to %s ..."), targetStr));
                    break;
                default:
                    SetStatusText(_("Disconnected"));
                    break;
            }
        }
    }
}

bool MainFrame::IsModified(int index) const {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return false;
    const auto& tab = m_tabs[index];
    if (!tab->activeProfile) return false;
    if (!tab->savedProfile) return true;
    return *tab->activeProfile != *tab->savedProfile;
}

wxString MainFrame::GetDisplayName(int index) const {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return _("Untitled");
    const auto& tab = m_tabs[index];
    if (tab->profilePath.IsEmpty()) {
        if (tab->protocol) {
            return wxString::FromUTF8(tab->protocol->DisplayName());
        }
        return _("Untitled");
    }
    return wxFileName(tab->profilePath).GetName();
}

void MainFrame::UpdateTabTitle(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    wxString title = GetDisplayName(index);
    if (IsModified(index)) title += " *";
    m_notebook->SetPageText(index, title);

    if (index == m_activeTabIndex) {
        wxString frameTitle = _("IndComm");
        if (!m_tabs[index]->profilePath.IsEmpty() || m_tabs[index]->activeProfile) {
            frameTitle += " - " + title;
        }
        SetTitle(frameTitle);
    }
}

void MainFrame::OnClose(wxCloseEvent& evt) {
    if (m_forceExit) {
        SaveRecentConnections();
        settings::UISettings::Instance().SaveMainWindowGeometry(GetScreenRect(), IsMaximized());

        SPDLOG_INFO(_("Application exiting"));

        for (int i = (int)m_tabs.size() - 1; i >= 0; --i) {
            RemoveTab(i);
        }

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

    if (!m_forceExit) {
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i]->activeProfile && IsModified(i)) {
                const int ret = wxMessageBox(
                    wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName(i)),
                    _("Save Connection Profile"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

                if (ret == wxCANCEL) {
                    evt.Veto();
                    return;
                }
                if (ret == wxYES) {
                    SaveCurrentProfile(false, i);
                    if (IsModified(i)) {
                        evt.Veto();
                        return;
                    }
                }
            }
        }
    }

    if (settings::UISettings::Instance().GetMinimizeToTrayOnClose()) {
        evt.Veto();
        MinimizeToTray();
        return;
    }

    m_forceExit = true;
    Close(true);
}

void MainFrame::OnQuit(wxCommandEvent&) {
    m_forceExit = true;
    Close(true);
}

void MainFrame::OnAbout(wxCommandEvent&) {
    wxMessageBox(_("IndComm\nA tool for testing industrial protocols."), _("About"),
                 wxOK | wxICON_INFORMATION, this);
    SPDLOG_INFO(_("About dialog shown"));
}

void MainFrame::OnShowLog(wxCommandEvent&) {
    if (!m_logFrame) {
        m_logFrame = new LogFrame(this);
    }
    m_logFrame->Show();
    m_logFrame->Raise();
}

void MainFrame::OnShowSettings(wxCommandEvent&) {
    SettingsDialog dlg(this);
    dlg.SetApplyCallback([this]() {
        for (auto& tab : m_tabs) {
            if (tab->panel) tab->panel->ApplyGlobalSettings();
        }
    });
    dlg.ShowModal();
}

void MainFrame::OnOpenConnection(wxCommandEvent&) {
    const wxString defaultDir = wxStandardPaths::Get().GetDocumentsDir();
    wxFileDialog dlg(this, _("Open Connection Configuration"), defaultDir, "",
                     "Connection files (*.json)|*.json|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    const wxString path = dlg.GetPath();

    ActiveProfile profile;
    if (!ConnectionProfile::Load(path, profile)) {
        wxMessageBox(wxString::Format(_("Failed to load connection profile:\n%s"), path),
                     _("Error"), wxOK | wxICON_ERROR, this);
        return;
    }

    AddRecentConnection(path);

    auto* proto = ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) return;

    ISessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;

    AddTab(proto, profile.modeIndex, std::move(session), profile, path);
    m_lastDialogProfile = profile;
}

void MainFrame::OnCloseConnection(wxCommandEvent&) {
    int index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return;

    auto& tab = m_tabs[index];
    if (tab->activeProfile && IsModified(index)) {
        const int ret = wxMessageBox(
            wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName(index)),
            _("Save Connection Profile"), wxYES_NO | wxCANCEL | wxICON_QUESTION, this);

        if (ret == wxCANCEL) return;
        if (ret == wxYES) {
            SaveCurrentProfile(false, index);
            if (IsModified(index)) return;
        }
    }

    RemoveTab(index);
}

void MainFrame::OnRecentConnection(wxCommandEvent& evt) {
    size_t index = static_cast<size_t>(evt.GetId() - idMenuRecentBase);
    if (index >= m_recentConnections.GetCount()) return;

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

    AddRecentConnection(path);

    auto* proto = ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) return;

    ISessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;

    AddTab(proto, profile.modeIndex, std::move(session), profile, path);
    m_lastDialogProfile = profile;
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

void MainFrame::SaveCurrentProfile(bool saveAs, int index) {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return;
    auto& tab = m_tabs[index];
    if (!tab->activeProfile) return;

    auto* proto = ProtocolRegistry::Instance().Find(tab->activeProfile->protocolId);
    if (!proto) return;

    if (saveAs || tab->profilePath.IsEmpty()) {
        const wxString defaultName =
            tab->profilePath.IsEmpty()
                ? ConnectionProfile::MakeDefaultFileName(proto, tab->activeProfile->modeIndex,
                                                         tab->activeProfile->fields)
                : wxFileName(tab->profilePath).GetFullName();

        wxString defaultDir = tab->profilePath.IsEmpty() ? wxStandardPaths::Get().GetDocumentsDir()
                                                         : wxFileName(tab->profilePath).GetPath();

        wxFileDialog dlg(this, _("Save Connection Profile"), defaultDir, defaultName,
                         "Connection files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return;

        wxString path = dlg.GetPath();
        if (path.IsEmpty()) return;

        wxFileName fn(path);
        if (fn.GetExt().Lower() != "json") {
            fn.SetExt("json");
            path = fn.GetFullPath();
        }

        tab->profilePath = path;
    }

    if (ConnectionProfile::Save(tab->profilePath, tab->activeProfile->protocolId,
                                tab->activeProfile->modeIndex, proto, tab->activeProfile->fields,
                                tab->activeProfile->session)) {
        tab->savedProfile = tab->activeProfile;
        AddRecentConnection(tab->profilePath);
        UpdateTabTitle(index);
        ICU_SPDLOG_INFO(_("Profile saved: %s"), tab->profilePath);
    } else {
        wxMessageBox(_("Failed to save connection profile.\n\nPlease check if the target folder is "
                       "writable, or blocked by Windows Defender."),
                     _("Error"), wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnSaveConnection(wxCommandEvent&) {
    SPDLOG_INFO(_("User clicked Save Connection"));
    SaveCurrentProfile(false);
}

void MainFrame::OnSaveConnectionAs(wxCommandEvent&) {
    SPDLOG_INFO(_("User clicked Save Connection As"));
    SaveCurrentProfile(true);
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

    ISessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;

    AddTab(proto, profile.modeIndex, std::move(session), profile, wxEmptyString);
}

}  // namespace icu
