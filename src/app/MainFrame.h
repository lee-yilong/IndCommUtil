#pragma once

#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/simplebook.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/toolbar.h>
#include <wx/wx.h>

#include <functional>
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
class WelcomePanel;
class CustomPopupMenu;

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

    idMenuEditAdd = 3000,
    idMenuEditCopy,
    idMenuEditEdit,
    idMenuEditDelete,

    idTbNew = 4000,
    idTbOpen,
    idTbSave,
    idTbSettings,
    idTbLog,
    idTbMinimize,
    idTbMaximize,
    idTbCloseWin
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

// ================= 自定义标签栏（替代 wxAuiNotebook） =================
class TabItem : public wxPanel {
public:
    TabItem(wxWindow* parent, const wxString& title, bool active,
            std::function<void()> onSelect, std::function<void()> onClose);

    void SetActive(bool active);
    void SetTitle(const wxString& title);
    void SetClosable(bool closable);

private:
    void OnPaint(wxPaintEvent& evt);
    void UpdateColours();

    wxStaticText* m_text = nullptr;
    wxStaticBitmap* m_closeBtn = nullptr;
    bool m_active = false;
    std::function<void()> m_onSelect;
    std::function<void()> m_onClose;
};

class TabBook : public wxPanel {
public:
    explicit TabBook(wxWindow* parent);

    void BindPageChanged(std::function<void(int)> cb);

    void AddPage(wxWindow* page, const wxString& text, bool select = false);
    void InsertPage(size_t index, wxWindow* page, const wxString& text, bool select = false);
    void DeletePage(size_t index);
    void RemovePage(size_t index);
    void SetSelection(size_t index);

    int GetSelection() const;
    size_t GetPageCount() const;
    wxWindow* GetPage(size_t index) const;
    int GetPageIndex(wxWindow* page) const;
    void SetPageText(size_t index, const wxString& text);

    // MainFrame 需要直接挂接“关闭页”回调
    std::function<void(int)> m_onClose;

private:
    struct PageEntry {
        wxWindow* window = nullptr;
        TabItem* tab = nullptr;
    };

    wxPanel* m_tabBar = nullptr;
    wxBoxSizer* m_tabSizer = nullptr;
    wxSimplebook* m_book = nullptr;
    std::vector<PageEntry> m_pages;
    std::function<void(int)> m_onPageChanged;

    // 只剩一个页签时隐藏它的关闭按钮（最后一个页面不允许关闭）
    void UpdateCloseButtons();
};

class MainFrame : public wxFrame {
public:
    MainFrame(wxFrame* parent, const wxString& title);
    ~MainFrame() override;

private:
    wxStaticBitmap* m_maxBtn = nullptr;
    wxStaticText* m_centerTitleText = nullptr;

    wxPanel* m_statusPanel = nullptr;
    wxStaticText* m_statusLeftText = nullptr;
    wxStaticText* m_statusRightText = nullptr;

    TabBook* m_notebook = nullptr;
    WelcomePanel* m_welcomePage = nullptr;

    std::vector<std::unique_ptr<TabContext>> m_tabs;
    int m_activeTabIndex = -1;

    LogFrame* m_logFrame = nullptr;
    AppTaskBarIcon* m_trayIcon = nullptr;
    bool m_forceExit = false;

    std::map<int, IProtocol*> m_protocolMenuMap;

    wxArrayString m_recentConnections;
    static constexpr size_t MAX_RECENT = 5;

    std::optional<ActiveProfile> m_lastDialogProfile;

    wxPoint m_dragStart;
    wxPoint m_winStart;

    wxBitmapBundle m_maxIcon;
    wxBitmapBundle m_restoreIcon;

    wxPanel* m_fileBtn = nullptr;
    wxStaticBitmap* m_fileArrow = nullptr;
    wxPanel* m_editBtn = nullptr;
    wxStaticBitmap* m_editArrow = nullptr;

    void SetupDrag(wxWindow* target);

    wxPanel* CreateDropdownBtn(wxWindow* parent, const wxString& iconName, const wxString& text,
                               wxStaticBitmap** outArrow);

    void ShowFileMenu(wxWindow* anchor);
    void ShowEditMenu(wxWindow* anchor);

    void OnClose(wxCloseEvent& evt);
    void OnQuit(wxCommandEvent& evt);
    void OnAbout(wxCommandEvent& evt);
    void OnShowLog(wxCommandEvent& evt);
    void OnShowSettings(wxCommandEvent& evt);
    void OnShowLanguageDialog();

    void OnOpenConnection(wxCommandEvent& evt);
    void OnSaveConnection(wxCommandEvent& evt);
    void OnSaveConnectionAs(wxCommandEvent& evt);
    void OnCloseConnection(wxCommandEvent& evt);
    void OnRecentConnection(wxCommandEvent& evt);

    void OnMenuOpenProtocol(wxCommandEvent& evt);

    void OnLangZhCN(wxCommandEvent& evt);
    void OnLangEnUS(wxCommandEvent& evt);

    void OnEditAdd(wxCommandEvent& evt);
    void OnEditCopy(wxCommandEvent& evt);
    void OnEditEdit(wxCommandEvent& evt);
    void OnEditDelete(wxCommandEvent& evt);

    void BuildLayout();
    void UpdateUIState();
    void AddRecentConnection(const wxString& path);
    void LoadRecentConnections();
    void SaveRecentConnections();

    void AddTab(IProtocol* proto, int modeIndex, ISessionPtr session, const ActiveProfile& profile,
                const wxString& path);
    void RemoveTab(int index, bool skipNotebookDelete = false);
    TabContext* GetActiveTab();

    void SetupPanelEvents(int index);

    bool IsModified(int index = -1) const;
    wxString GetDisplayName(int index = -1) const;
    void UpdateTabTitle(int index);
    void SaveCurrentProfile(bool saveAs = false, int index = -1);

    void MinimizeToTray();
    void RestoreFromTray();

    void OnTabChanged(int sel);

    void OnSize(wxSizeEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
