#pragma once

#include <wx/font.h>

namespace icu::settings {

class FontSettings {
public:
    static FontSettings& Instance();

    wxFont GetUiFont() const;
    wxFont GetLogFont() const;
    wxFont GetDataFont() const;

    void SetUiFont(const wxFont&);
    void SetLogFont(const wxFont&);
    void SetDataFont(const wxFont&);

    static wxFont SystemDefault();
    static wxFont MonospaceDefault();

private:
    FontSettings() = default;
};

}  // namespace icu::settings
