#include "Settings.h"

#include <wx/config.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

#include <vector>

#include "log/Log.h"

namespace icu::settings {

Settings& Settings::Instance() {
    static Settings inst;
    return inst;
}

void Settings::Init() {
    if (wxConfigBase::Get(false)) return;  // 已初始化

    // 按优先级列出候选位置
    std::vector<wxString> candidates;

    // 1. 用户配置目录：%APPDATA%/IndustrialCommunicationUtility
    candidates.push_back(wxStandardPaths::Get().GetUserConfigDir() +
                         wxFileName::GetPathSeparator() + "IndustrialCommunicationUtility");

    // 2. 用户本地数据目录（wxWidgets 在 Windows 上通常是 %LOCALAPPDATA%）
    candidates.push_back(wxStandardPaths::Get().GetUserLocalDataDir());

    // 3. 可执行文件所在目录（build/）
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    candidates.push_back(exePath.GetPath());

    // 4. 当前工作目录
    candidates.push_back(wxGetCwd());

    // 探测阶段屏蔽 wx 的错误弹窗（某些路径不可写会弹 "Failed to create configuration file
    // directory"）
    {
        wxLogNull noLog;

        for (const auto& dir : candidates) {
            if (dir.IsEmpty()) continue;

            if (!wxFileName::DirExists(dir)) {
                if (!wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) continue;
            }

            const wxString path =
                dir + wxFileName::GetPathSeparator() + "IndustrialCommunicationUtility.ini";

            auto* fc = new wxFileConfig(wxEmptyString, wxEmptyString, path, wxEmptyString,
                                        wxCONFIG_USE_LOCAL_FILE);

            // 实际测试写入能力：写一个探针 key 再删掉
            if (fc->Write("/Settings/CanWrite", "1")) {
                fc->DeleteEntry("/Settings/CanWrite");
                fc->Flush();

                wxConfigBase::Set(fc);
                SPDLOG_INFO(_("Settings initialized"));
                return;
            }

            delete fc;
        }
    }

    SPDLOG_ERROR(_("Failed to initialize settings: no writable location"));
}

void Settings::Shutdown() {
    if (auto* c = wxConfigBase::Get(false)) {
        c->Flush();
        wxConfigBase::Set(nullptr);
    }
    SPDLOG_INFO(_("Settings shutdown"));
}

wxConfigBase* Settings::Raw() const {
    return wxConfigBase::Get(false);
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
