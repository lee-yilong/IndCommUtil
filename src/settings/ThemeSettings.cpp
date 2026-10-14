#include "ThemeSettings.h"

#include "Settings.h"
#include "SettingsKeys.h"

namespace icu::settings {

ThemeSettings& ThemeSettings::Instance() {
    static ThemeSettings inst;
    return inst;
}

Theme ThemeSettings::GetTheme() const {
    const wxString v = Settings::Instance().GetString(kUiTheme, "system");
    if (v == "light") return Theme::Light;
    if (v == "dark") return Theme::Dark;
    return Theme::System;
}

void ThemeSettings::SetTheme(Theme t) {
    wxString v = "system";
    if (t == Theme::Light) v = "light";
    if (t == Theme::Dark) v = "dark";
    Settings::Instance().SetString(kUiTheme, v);
}

void ThemeSettings::Apply() {}

}  // namespace icu::settings
