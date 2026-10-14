#pragma once

#include <wx/app.h>
#include <wx/string.h>

namespace icu {

class App : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;

    void SwitchLanguage(const wxString& canonicalName);
    wxString GetCurrentLanguage() const;
};

}  // namespace icu
