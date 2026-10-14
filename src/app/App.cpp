#include "app/App.h"

#include <wx/msgdlg.h>
#include <wx/stdpaths.h>

#include "app/MainFrame.h"
#include "log/Log.h"
#include "core/ProtocolRegistry.h"
#include "settings/LanguageSettings.h"
#include "settings/Settings.h"
#include "spdlog/spdlog.h"

namespace icu {

bool App::OnInit() {
    SPDLOG_INFO(_("Application initializing..."));

    settings::LanguageSettings::SetupUtf8Locale();
    settings::Settings::Instance().Init();
    log::Init();

    auto& lang = settings::LanguageSettings::Instance();
    lang.ApplyLanguage(lang.GetSavedLanguage());

    RegisterBuiltinProtocols();

    auto* frame = new MainFrame(nullptr, _("Industrial Communication Utility"));
    frame->Show(true);

    SPDLOG_INFO(_("Application initialized"));
    return true;
}

int App::OnExit() {
    SPDLOG_INFO(_("Application shutting down..."));
    settings::Settings::Instance().Shutdown();
    log::Shutdown();
    return wxApp::OnExit();
}

void App::SwitchLanguage(const wxString& canonicalName) {
    auto& lang = settings::LanguageSettings::Instance();

    if (canonicalName == lang.CurrentLanguage()) {
        ICU_SPDLOG_INFO(_("Language already '%s', ignoring"), canonicalName);
        return;
    }

    lang.SaveLanguage(canonicalName);

    const int ret = wxMessageBox(_("Language will be applied after restart.\n\nRestart now?"),
                                 _("Language Changed"), wxYES_NO | wxICON_QUESTION);

    if (ret == wxYES) {
        const wxString exe = wxStandardPaths::Get().GetExecutablePath();
        ICU_SPDLOG_INFO(_("Restarting application: %s"), exe);
        wxExecute("\"" + exe + "\"", wxEXEC_ASYNC);
        ExitMainLoop();
    } else {
        SPDLOG_INFO(_("Language will be applied on next start"));
    }
}

wxString App::GetCurrentLanguage() const {
    return settings::LanguageSettings::Instance().CurrentLanguage();
}

}  // namespace icu
