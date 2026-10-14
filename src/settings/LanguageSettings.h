#pragma once

#include <wx/string.h>

#include <memory>

class wxLocale;

namespace icu::settings {

class LanguageSettings {
public:
    static LanguageSettings& Instance();

    wxString GetSavedLanguage() const;
    void SaveLanguage(const wxString& canonicalName);
    bool ApplyLanguage(const wxString& canonicalName);
    wxString CurrentLanguage() const { return m_currentLang; }

    static void SetupUtf8Locale();

private:
    LanguageSettings() = default;

    std::unique_ptr<wxLocale> m_locale;
    wxString m_currentLang;
};

}  // namespace icu::settings
