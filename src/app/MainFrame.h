#pragma once

#include <wx/simplebook.h>
#include <wx/wx.h>

#include <map>
#include <optional>

#include "core/IProtocol.h"
#include "core/ProtocolTypes.h"
#include "core/SessionManager.h"

namespace icu {

class ProtocolSessionPanel;
class LogFrame;
class AppTaskBarIcon;
class SessionPanel;

enum MenuId {
    idMenuQuit = 1000,
    idMenuAbout,
    idMenuLog,

    idMenuOpenConnection,
    idMenuSaveConnection,
    idMenuSaveConnectionAs,
    idMenuCloseConnection,

    idMenuNewProtocolBase = 1100,
    idMenuNewProtocolEnd = 1200,

    idMenuLangZhCN,
    idMenuLangEnUS,

    idMenuRecentBase = 2000,
};

class MainFrame : public wxFrame {
public:
    MainFrame(wxFrame* parent, const wxString& title);
    ~MainFrame() override;

private:
    wxTextCtrl* m_logTextCtrl = nullptr;

    wxMenu* m_fileMenu = nullptr;
    wxMenu* m_recentMenu = nullptr;
    wxMenuItem* m_recentMenuItem = nullptr;

    wxSimplebook* m_contentBook = nullptr;
    SessionPanel* m_sessionPanel = nullptr;

    LogFrame* m_logFrame = nullptr;
    AppTaskBarIcon* m_trayIcon = nullptr;
    bool m_forceExit = false;

    SessionManager m_sessionMgr;

    std::map<int, IProtocol*> m_protocolMenuMap;

    wxArrayString m_recentConnections;
    static constexpr size_t MAX_RECENT = 5;

    std::optional<ActiveProfile> m_lastDialogProfile;
    std::optional<ActiveProfile> m_activeProfile;
    std::optional<ActiveProfile> m_savedProfile;
    wxString m_currentProfilePath;

    void OnClose(wxCloseEvent& evt);
    void OnQuit(wxCommandEvent& evt);
    void OnAbout(wxCommandEvent& evt);
    void OnShowLog(wxCommandEvent& evt);

    void OnOpenConnection(wxCommandEvent& evt);
    void OnSaveConnection(wxCommandEvent& evt);
    void OnSaveConnectionAs(wxCommandEvent& evt);
    void OnCloseConnection(wxCommandEvent& evt);
    void OnRecentConnection(wxCommandEvent& evt);

    void OnMenuOpenProtocol(wxCommandEvent& evt);

    void OnLangZhCN(wxCommandEvent& evt);
    void OnLangEnUS(wxCommandEvent& evt);

    void BuildMenuBar();
    void BuildLayout();
    void UpdateConnectionUI();
    void AddRecentConnection(const wxString& path);
    void RebuildRecentMenu();
    void LoadRecentConnections();
    void SaveRecentConnections();

    void ShowWelcomePage();
    void ShowSessionPage(IProtocol* proto, int modeIndex, ISession* session);

    IProtocol* GetActiveProtocol() const;
    void CreateAndOpenSession(const ActiveProfile& profile);

    bool IsModified() const;
    wxString GetDisplayName() const;
    void UpdateTitle();
    void SaveCurrentProfile();
    void SaveCurrentProfileAs();

    void MinimizeToTray();
    void RestoreFromTray();

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
