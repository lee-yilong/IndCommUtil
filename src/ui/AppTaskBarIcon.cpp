// =========================================================================
// 必须在所有 wx 头文件之前 include windows.h 并 #undef GetClassInfo
//
// 原因：windows.h 会定义
//     #define GetClassInfo GetClassInfoW
// 这会污染 wxWidgets 里的 virtual 方法名，导致 override 检查失败。
// 提前 include + undef，避免冲突。
// =========================================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

#include <wx/artprov.h>
#include <wx/icon.h>

#include "AppTaskBarIcon.h"
#include "log/Log.h"

namespace icu {

enum {
    idTrayShow = wxID_HIGHEST + 100,
    idTrayExit,
};

// clang-format off
// wxWidgets 事件表宏展开后是一条语句，clang-format 会把后续行的缩进逐级累积
wxBEGIN_EVENT_TABLE(AppTaskBarIcon, wxTaskBarIcon) EVT_MENU(idTrayShow, AppTaskBarIcon::OnMenuShow)
    EVT_MENU(idTrayExit, AppTaskBarIcon::OnMenuExit)
    EVT_TASKBAR_LEFT_DCLICK(AppTaskBarIcon::OnDoubleClick) wxEND_EVENT_TABLE();
// clang-format on

AppTaskBarIcon::AppTaskBarIcon() {
#ifdef _WIN32
    HINSTANCE hInst = ::GetModuleHandleW(nullptr);

    // 优先从 exe 资源段加载自定义图标，明确指定尺寸
    HICON hIcon = static_cast<HICON>(
        ::LoadImageW(hInst, L"aaaa_app_icon", IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON),
                     ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

    if (hIcon == nullptr) {
        // 降级：系统默认应用图标
        hIcon = static_cast<HICON>(::LoadImageW(nullptr, MAKEINTRESOURCEW(32512), IMAGE_ICON,
                                                ::GetSystemMetrics(SM_CXSMICON),
                                                ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    }

    if (hIcon == nullptr) {
        SPDLOG_WARN(_("No valid icon for taskbar; tray icon unavailable"));
        return;
    }

    wxIcon icon;
    icon.CreateFromHICON(hIcon);
    if (!icon.IsOk()) {
        SPDLOG_WARN(_("Failed to create wxIcon from HICON"));
        ::DestroyIcon(hIcon);
        return;
    }

    SetIcon(icon, _("Industrial Communication Utility"));
    SPDLOG_INFO(_("Taskbar icon set successfully"));  // 确认到达
#else
    wxIcon icon = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16));
    if (!icon.IsOk()) {
        SPDLOG_WARN(_("No valid icon for taskbar; tray icon unavailable"));
        return;
    }
    SetIcon(icon, _("Industrial Communication Utility"));
#endif
}

wxMenu* AppTaskBarIcon::CreatePopupMenu() {
    auto* menu = new wxMenu();
    menu->Append(idTrayShow, _("Show Window"));
    menu->AppendSeparator();
    menu->Append(idTrayExit, _("Exit"));
    return menu;
}

void AppTaskBarIcon::OnMenuShow(wxCommandEvent&) {
    if (m_showCb) m_showCb();
}

void AppTaskBarIcon::OnMenuExit(wxCommandEvent&) {
    if (m_exitCb) m_exitCb();
}

void AppTaskBarIcon::OnDoubleClick(wxTaskBarIconEvent&) {
    if (m_showCb) m_showCb();
}

}  // namespace icu
