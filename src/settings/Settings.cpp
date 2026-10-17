#include "Settings.h"

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "log/Log.h"

namespace icu::settings {

namespace {

using json = nlohmann::json;

constexpr const char* kAppDirName = "IndustrialCommunicationUtility";
constexpr const char* kFileName = "settings.json";

std::string g_path;            // settings.json 的完整路径
json g_data = json::object();  // 内存中的配置（平坦结构）
std::mutex g_mutex;

// wxConfig 风格的 key 是 "/Settings/Language"，
// 我们存成平坦的 "Settings/Language"，去掉开头的 '/'
std::string NormalizeKey(const wxString& key) {
    std::string k(key.utf8_str());
    while (!k.empty() && k.front() == '/') k.erase(k.begin());
    return k;
}

// 探测可写位置，返回 settings.json 的完整路径
// 注意：wxStandardPaths 和 wxFileName 在失败时会 wxLogError 弹框，
//       必须用 wxLogNull 屏蔽。
std::string FindWritablePath() {
    wxLogNull noLog;

    std::vector<wxString> candidates;

    // 1. %APPDATA%/IndustrialCommunicationUtility
    const wxString userConfigDir = wxStandardPaths::Get().GetUserConfigDir();
    if (!userConfigDir.IsEmpty()) {
        candidates.push_back(userConfigDir + wxFileName::GetPathSeparator() + kAppDirName);
    }

    // 2. %LOCALAPPDATA%/IndustrialCommunicationUtility
    wxString localAppData;
    if (wxGetEnv("LOCALAPPDATA", &localAppData) && !localAppData.IsEmpty()) {
        candidates.push_back(localAppData + wxFileName::GetPathSeparator() + kAppDirName);
    }

    // 3. %USERPROFILE%/.IndustrialCommunicationUtility
    wxString userProfile;
    if (wxGetEnv("USERPROFILE", &userProfile) && !userProfile.IsEmpty()) {
        candidates.push_back(userProfile + wxFileName::GetPathSeparator() +
                             ".IndustrialCommunicationUtility");
    }

    // ★ 删掉了 exe 目录 / cwd 两个候选！
    // 写配置到 exe 目录会导致 Windows Shell 判定该目录"被应用污染"，
    // 触发 SmartScreen 警告 + Shell IFileDialog 拒绝保存 + 托盘失败。

    for (const auto& dir : candidates) {
        if (dir.IsEmpty()) continue;

        if (!wxFileName::DirExists(dir)) {
            if (!wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
                continue;
            }
        }

        wxString path = dir + wxFileName::GetPathSeparator() + kFileName;
        std::string utf8 = std::string(path.utf8_str());

        std::ofstream probe(utf8, std::ios::app);
        if (!probe.is_open()) continue;
        probe.close();

        return utf8;
    }
    return {};
}

void SaveLocked() {
    if (g_path.empty()) return;

    std::ofstream out(g_path, std::ios::trunc);
    if (!out.is_open()) {
        ICU_SPDLOG_WARN(_("Failed to open settings file for write: %s"),
                        wxString::FromUTF8(g_path.c_str()));
        return;
    }
    out << g_data.dump(2);
}

}  // namespace

Settings& Settings::Instance() {
    static Settings inst;
    return inst;
}

void Settings::Init() {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_path.empty()) return;

    g_path = FindWritablePath();
    if (g_path.empty()) {
        SPDLOG_ERROR(_("Failed to initialize settings: no writable location"));
        return;
    }

    std::ifstream in(g_path);
    if (in.is_open()) {
        try {
            in >> g_data;
            if (!g_data.is_object()) g_data = json::object();
        } catch (const std::exception& e) {
            ICU_SPDLOG_WARN(_("Failed to parse settings file: %s"), wxString::FromUTF8(e.what()));
            g_data = json::object();
        }
    }

    ICU_SPDLOG_INFO(_("Settings initialized: %s"), wxString::FromUTF8(g_path.c_str()));
}

void Settings::Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    SaveLocked();
    g_data = json::object();
    g_path.clear();
    SPDLOG_INFO(_("Settings shutdown"));
}

wxString Settings::GetString(const wxString& key, const wxString& def) const {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return def;
    auto it = g_data.find(k);
    if (it == g_data.end() || !it->is_string()) return def;
    return wxString::FromUTF8(it->get<std::string>().c_str());
}

void Settings::SetString(const wxString& key, const wxString& value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return;
    g_data[k] = std::string(value.utf8_str());
}

int Settings::GetInt(const wxString& key, int def) const {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return def;
    auto it = g_data.find(k);
    if (it == g_data.end()) return def;
    if (it->is_number_integer()) return it->get<int>();
    if (it->is_string()) {
        try {
            return std::stoi(it->get<std::string>());
        } catch (...) {
            return def;
        }
    }
    return def;
}

void Settings::SetInt(const wxString& key, int value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return;
    g_data[k] = value;
}

bool Settings::GetBool(const wxString& key, bool def) const {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return def;
    auto it = g_data.find(k);
    if (it == g_data.end()) return def;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number_integer()) return it->get<int>() != 0;
    if (it->is_string()) {
        const std::string s = it->get<std::string>();
        return (s == "1" || s == "true" || s == "yes");
    }
    return def;
}

void Settings::SetBool(const wxString& key, bool value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return;
    g_data[k] = value;
}

void Settings::DeleteKey(const wxString& key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string k = NormalizeKey(key);
    if (k.empty()) return;
    g_data.erase(k);
}

void Settings::DeleteGroup(const wxString& group) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string prefix = NormalizeKey(group);
    if (prefix.empty()) return;

    const std::string withSlash = prefix + "/";

    for (auto it = g_data.begin(); it != g_data.end();) {
        const std::string& k = it.key();
        if (k == prefix || k.compare(0, withSlash.size(), withSlash) == 0) {
            it = g_data.erase(it);
        } else {
            ++it;
        }
    }
}

void Settings::Flush() {
    std::lock_guard<std::mutex> lock(g_mutex);
    SaveLocked();
}

wxConfigBase* Settings::Raw() const {
    return nullptr;
}

}  // namespace icu::settings
