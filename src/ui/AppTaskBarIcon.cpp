#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/icon.h>
#include <wx/stdpaths.h>

#include "AppTaskBarIcon.h"
#include "log/Log.h"

namespace icu {

enum {
    idTrayShow = wxID_HIGHEST + 100,
    idTrayExit,
};

// clang-format off
wxBEGIN_EVENT_TABLE(AppTaskBarIcon, wxTaskBarIcon) EVT_MENU(idTrayShow, AppTaskBarIcon::OnMenuShow)
    EVT_MENU(idTrayExit, AppTaskBarIcon::OnMenuExit)
    EVT_TASKBAR_LEFT_DCLICK(AppTaskBarIcon::OnDoubleClick) wxEND_EVENT_TABLE();
// clang-format on

AppTaskBarIcon::AppTaskBarIcon() {
#ifdef _WIN32
    HINSTANCE hInst = ::GetModuleHandleW(nullptr);
    HICON hIcon =
        (HICON)::LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON),
                            ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (hIcon) {
        wxIcon icon;
        icon.CreateFromHICON(hIcon);
        SetIcon(icon, _("Industrial Communication Utility"));
    } else {
        wxIcon fallback = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16));
        SetIcon(fallback, _("Industrial Communication Utility"));
    }
#else
    wxIcon icon = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16));
    if (icon.IsOk()) SetIcon(icon, _("Industrial Communication Utility"));
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
