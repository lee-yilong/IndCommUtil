#pragma once

#include <wx/wx.h>

#include <functional>

namespace icu {

class WelcomePanel : public wxPanel {
public:
    WelcomePanel(wxWindow* parent, const wxArrayString& recentFiles);
    ~WelcomePanel() override = default;

    std::function<void()> m_onNewClick;
    std::function<void()> m_onOpenClick;
    std::function<void(const wxString&)> m_onRecentClick;

private:
    wxArrayString m_recentFiles;
    void BuildUi();
};

}  // namespace icu
