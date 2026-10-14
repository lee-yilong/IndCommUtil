#include "LanguageSettings.h"

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>
#include <wx/translation.h>

#include <clocale>

#include "Settings.h"
#include "SettingsKeys.h"
#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu::settings {

namespace {

constexpr const char* kCatalog = "IndustrialCommunicationUtility";

wxString GetI18nDir() {
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    return exePath.GetPath() + "/resources/i18n";
}

wxString NormalizeLangName(const wxString& raw) {
    if (raw.StartsWith("zh")) return "zh_CN";
    if (raw.StartsWith("en")) return "en_US";
    return raw;
}

}  // namespace

LanguageSettings& LanguageSettings::Instance() {
    static LanguageSettings inst;
    return inst;
}

void LanguageSettings::SetupUtf8Locale() {
    const char* candidates[] = {".UTF-8", ".UTF8", "C.UTF-8", "en_US.UTF-8"};
    for (const char* loc : candidates) {
        if (std::setlocale(LC_ALL, loc)) return;
    }
    SPDLOG_WARN(_("UTF-8 locale not available"));
}

wxString LanguageSettings::GetSavedLanguage() const {
    wxString lang = Settings::Instance().GetString(kLanguage);

    if (lang.IsEmpty()) {
        const int sysLang = wxLocale::GetSystemLanguage();
        if (const wxLanguageInfo* info = wxLocale::GetLanguageInfo(sysLang))
            lang = NormalizeLangName(info->CanonicalName);
        else
            lang = "zh_CN";
    }
    return lang;
}

void LanguageSettings::SaveLanguage(const wxString& canonicalName) {
    Settings::Instance().SetString(kLanguage, canonicalName);
    Settings::Instance().Flush();
    ICU_SPDLOG_INFO(_("Language preference saved: %s"), canonicalName);
}

bool LanguageSettings::ApplyLanguage(const wxString& canonicalName) {
    ICU_SPDLOG_INFO(_("Applying language: %s"), canonicalName);

    m_locale.reset();
    m_locale = std::make_unique<wxLocale>();

    const wxLanguageInfo* info = wxLocale::FindLanguageInfo(canonicalName);
    bool ok = info ? m_locale->Init(info->Language) : m_locale->Init(canonicalName);

    if (!ok) {
        ICU_SPDLOG_WARN(_("wxLocale::Init failed for %s"), canonicalName);
        m_locale->Init(wxLANGUAGE_DEFAULT);
    }

    wxFileTranslationsLoader::AddCatalogLookupPathPrefix(GetI18nDir());
    if (!m_locale->AddCatalog(kCatalog)) {
        ICU_SPDLOG_WARN(_("Failed to load catalog '%s' for lang '%s'"),
                        wxString::FromUTF8(kCatalog), canonicalName);
    } else {
        ICU_SPDLOG_INFO(_("Catalog '%s' loaded for lang '%s'"), wxString::FromUTF8(kCatalog),
                        canonicalName);
    }

    m_currentLang = canonicalName;
    return true;
}

}  // namespace icu::settings
