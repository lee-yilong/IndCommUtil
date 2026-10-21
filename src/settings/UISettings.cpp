#include "UISettings.h"

#include "Settings.h"
#include "SettingsKeys.h"

namespace icu::settings {

UISettings& UISettings::Instance() {
    static UISettings inst;
    return inst;
}

UISettings::WindowGeometry UISettings::LoadMainWindowGeometry() const {
    auto& s = Settings::Instance();

    WindowGeometry g;
    const int x = s.GetInt(kMainWindowX, 0);
    const int y = s.GetInt(kMainWindowY, 0);
    const int w = s.GetInt(kMainWindowW, 0);
    const int h = s.GetInt(kMainWindowH, 0);

    if (w > 0 && h > 0) {
        g.rect = wxRect(x, y, w, h);
        g.maximized = s.GetBool(kMainWindowMaximized, false);
        g.valid = true;
    }
    return g;
}

void UISettings::SaveMainWindowGeometry(const wxRect& rect, bool maximized) {
    auto& s = Settings::Instance();
    s.SetInt(kMainWindowX, rect.x);
    s.SetInt(kMainWindowY, rect.y);
    s.SetInt(kMainWindowW, rect.width);
    s.SetInt(kMainWindowH, rect.height);
    s.SetBool(kMainWindowMaximized, maximized);
    s.Flush();
}

UISettings::WindowGeometry UISettings::LoadLogWindowGeometry() const {
    auto& s = Settings::Instance();

    WindowGeometry g;
    const int x = s.GetInt(kLogWindowX, 0);
    const int y = s.GetInt(kLogWindowY, 0);
    const int w = s.GetInt(kLogWindowW, 0);
    const int h = s.GetInt(kLogWindowH, 0);

    if (w > 0 && h > 0) {
        g.rect = wxRect(x, y, w, h);
        g.valid = true;
    }
    return g;
}

void UISettings::SaveLogWindowGeometry(const wxRect& rect) {
    auto& s = Settings::Instance();
    s.SetInt(kLogWindowX, rect.x);
    s.SetInt(kLogWindowY, rect.y);
    s.SetInt(kLogWindowW, rect.width);
    s.SetInt(kLogWindowH, rect.height);
    s.Flush();
}

wxString UISettings::GetRecentConnectionsPacked() const {
    return Settings::Instance().GetString(kRecentConnections, "");
}

void UISettings::SetRecentConnectionsPacked(const wxString& packed) {
    Settings::Instance().SetString(kRecentConnections, packed);
    Settings::Instance().Flush();
}

int UISettings::GetFontSize() const {
    return Settings::Instance().GetInt(kUiFontSize, 10);
}

void UISettings::SetFontSize(int pt) {
    Settings::Instance().SetInt(kUiFontSize, pt);
}

bool UISettings::GetHexDisplay() const {
    return Settings::Instance().GetBool(kUiHexDisplay, false);
}

void UISettings::SetHexDisplay(bool on) {
    Settings::Instance().SetBool(kUiHexDisplay, on);
    Settings::Instance().Flush();
}

bool UISettings::GetAutoScroll() const {
    return Settings::Instance().GetBool(kUiAutoScroll, true);
}

void UISettings::SetAutoScroll(bool on) {
    Settings::Instance().SetBool(kUiAutoScroll, on);
    Settings::Instance().Flush();
}

bool UISettings::GetShowTimestamp() const {
    return Settings::Instance().GetBool(kUiShowTimestamp, true);
}

void UISettings::SetShowTimestamp(bool on) {
    Settings::Instance().SetBool(kUiShowTimestamp, on);
    Settings::Instance().Flush();
}

bool UISettings::GetMinimizeToTrayOnClose() const {
    return Settings::Instance().GetBool(kMinimizeToTrayOnClose, false);
}

void UISettings::SetMinimizeToTrayOnClose(bool v) {
    Settings::Instance().SetBool(kMinimizeToTrayOnClose, v);
    Settings::Instance().Flush();
}

}  // namespace icu::settings
