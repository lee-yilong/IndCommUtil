#pragma once

#include <wx/gdicmn.h>
#include <wx/string.h>

namespace icu::settings {

class UISettings {
public:
    static UISettings& Instance();

    struct WindowGeometry {
        wxRect rect;
        bool maximized = false;
        bool valid = false;
    };

    WindowGeometry LoadMainWindowGeometry() const;
    void SaveMainWindowGeometry(const wxRect& rect, bool maximized);

    WindowGeometry LoadLogWindowGeometry() const;
    void SaveLogWindowGeometry(const wxRect& rect);

    wxString GetRecentConnectionsPacked() const;
    void SetRecentConnectionsPacked(const wxString& packed);

    int GetFontSize() const;
    void SetFontSize(int pt);

    bool GetHexDisplay() const;
    void SetHexDisplay(bool on);

    bool GetAutoScroll() const;
    void SetAutoScroll(bool on);

    bool GetMinimizeToTrayOnClose() const;
    void SetMinimizeToTrayOnClose(bool v);

private:
    UISettings() = default;
};

}  // namespace icu::settings
