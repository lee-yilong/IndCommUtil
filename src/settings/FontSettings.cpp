#include "FontSettings.h"

#include <wx/fontenum.h>

#include "Settings.h"
#include "SettingsKeys.h"

namespace icu::settings {

FontSettings& FontSettings::Instance() {
    static FontSettings inst;
    return inst;
}

wxFont FontSettings::SystemDefault() {
    return wxFont(wxNORMAL_FONT->GetPointSize(), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                  wxFONTWEIGHT_NORMAL);
}

wxFont FontSettings::MonospaceDefault() {
    return wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
}

wxFont FontSettings::GetUiFont() const { return SystemDefault(); }
wxFont FontSettings::GetLogFont() const { return MonospaceDefault(); }
wxFont FontSettings::GetDataFont() const { return MonospaceDefault(); }

void FontSettings::SetUiFont(const wxFont&) {}
void FontSettings::SetLogFont(const wxFont&) {}
void FontSettings::SetDataFont(const wxFont&) {}

}  // namespace icu::settings
