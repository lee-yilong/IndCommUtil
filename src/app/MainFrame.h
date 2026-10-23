#pragma once

#include <wx/aui/auibook.h>
#include <wx/wx.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "core/IProtocol.h"
#include "core/ProtocolTypes.h"

namespace icu {

class SessionPanel;
class LogFrame;
class AppTaskBarIcon;

enum MenuId {
    idMenuQuit = 1000,
    idMenuAbout,
    idMenuLog,
    idMenuSettingsOptions,

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

struct TabContext {
    IProtocol* protocol = nullptr;
    int modeIndex = 0;
    ISessionPtr session;
    SessionPanel* panel = nullptr;
    std::optional<ActiveProfile> activeProfile;
    std::optional<ActiveProfile> savedProfile;
    wxString profilePath;
};

class MainFrame : public wxFrame {
public:
    MainFrame(wxFrame* parent, const wxString& title);
    ~MainFrame() override;

private:
    wxMenu* m_fileMenu = nullptr;
    wxMenu* m_recentMenu = nullptr;
    wxMenuItem* m_recentMenuItem = nullptr;

    wxAuiNotebook* m_notebook = nullptr;
    std::vector<std::unique_ptr<TabContext>> m_tabs;
    int m_activeTabIndex = -1;

    LogFrame* m_logFrame = nullptr;
    AppTaskBarIcon* m_trayIcon = nullptr;
    bool m_forceExit = false;

    std::map<int, IProtocol*> m_protocolMenuMap;

    wxArrayString m_recentConnections;
    static constexpr size_t MAX_RECENT = 5;

    std::optional<ActiveProfile> m_lastDialogProfile;

    void OnClose(wxCloseEvent& evt);
    void OnQuit(wxCommandEvent& evt);
    void OnAbout(wxCommandEvent& evt);
    void OnShowLog(wxCommandEvent& evt);
    void OnShowSettings(wxCommandEvent& evt);

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
    void UpdateUIState();
    void AddRecentConnection(const wxString& path);
    void RebuildRecentMenu();
    void LoadRecentConnections();
    void SaveRecentConnections();

    void AddTab(IProtocol* proto, int modeIndex, ISessionPtr session, const ActiveProfile& profile,
                const wxString& path);
    void RemoveTab(int index, bool skipNotebookDelete = false);
    TabContext* GetActiveTab();

    bool IsModified(int index = -1) const;
    wxString GetDisplayName(int index = -1) const;
    void UpdateTabTitle(int index);
    void SaveCurrentProfile(bool saveAs = false, int index = -1);

    void MinimizeToTray();
    void RestoreFromTray();

    void OnNotebookPageChanged(wxAuiNotebookEvent& evt);
    void OnNotebookPageClose(wxAuiNotebookEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
