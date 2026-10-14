#include "Settings.h"

#include <wx/config.h>

#include "log/Log.h"

namespace icu::settings {

Settings& Settings::Instance() {
    static Settings inst;
    return inst;
}

void Settings::Init() {
    if (wxConfigBase::Get()) return;  // 已初始化
    wxConfigBase::Set(new wxConfig("IndustrialCommunicationUtility"));
    SPDLOG_INFO(_("Settings initialized"));
}

void Settings::Shutdown() {
    if (auto* c = wxConfigBase::Get()) {
        c->Flush();
        wxConfigBase::Set(nullptr);
    }
    SPDLOG_INFO(_("Settings shutdown"));
}

wxConfigBase* Settings::Raw() const {
    return wxConfigBase::Get();
}

wxString Settings::GetString(const wxString& key, const wxString& def) const {
    wxString v;
    auto* c = Raw();
    if (!c) return def;
    c->Read(key, &v, def);
    return v;
}

void Settings::SetString(const wxString& key, const wxString& value) {
    if (auto* c = Raw()) c->Write(key, value);
}

int Settings::GetInt(const wxString& key, int def) const {
    auto* c = Raw();
    if (!c) return def;
    long v = def;
    c->Read(key, &v, def);
    return (int)v;
}

void Settings::SetInt(const wxString& key, int value) {
    if (auto* c = Raw()) c->Write(key, (long)value);
}

bool Settings::GetBool(const wxString& key, bool def) const {
    auto* c = Raw();
    if (!c) return def;
    bool v = def;
    c->Read(key, &v, def);
    return v;
}

void Settings::SetBool(const wxString& key, bool value) {
    if (auto* c = Raw()) c->Write(key, value);
}

void Settings::DeleteKey(const wxString& key) {
    if (auto* c = Raw()) c->DeleteEntry(key);
}

void Settings::DeleteGroup(const wxString& group) {
    if (auto* c = Raw()) c->DeleteGroup(group);
}

void Settings::Flush() {
    if (auto* c = Raw()) c->Flush();
}

}  // namespace icu::settings
