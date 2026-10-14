#include "AppTaskBarIcon.h"

#include <wx/artprov.h>

#include "log/Log.h"

namespace icu {

enum {
    idTrayShow = wxID_HIGHEST + 100,
    idTrayExit,
};

wxBEGIN_EVENT_TABLE(AppTaskBarIcon, wxTaskBarIcon) EVT_MENU(idTrayShow, AppTaskBarIcon::OnMenuShow)
    EVT_MENU(idTrayExit, AppTaskBarIcon::OnMenuExit)
        EVT_TASKBAR_LEFT_DCLICK(AppTaskBarIcon::OnDoubleClick) wxEND_EVENT_TABLE()

            AppTaskBarIcon::AppTaskBarIcon() {
    SetIcon(wxArtProvider::GetIcon(wxART_INFORMATION, wxART_OTHER, wxSize(16, 16)),
            _("Industrial Communication Utility"));
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
