#pragma once

#include <wx/menu.h>
#include <wx/taskbar.h>

#include <functional>
#include <string>

namespace icu {

class AppTaskBarIcon : public wxTaskBarIcon {
public:
    AppTaskBarIcon();

    using ShowCallback = std::function<void()>;
    void OnShow(ShowCallback cb) { m_showCb = std::move(cb); }

    using ExitCallback = std::function<void()>;
    void OnExit(ExitCallback cb) { m_exitCb = std::move(cb); }

protected:
    wxMenu* CreatePopupMenu() override;
    void OnMenuShow(wxCommandEvent& evt);
    void OnMenuExit(wxCommandEvent& evt);
    void OnDoubleClick(wxTaskBarIconEvent& evt);

private:
    ShowCallback m_showCb;
    ExitCallback m_exitCb;

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
