#pragma once

#include <wx/string.h>

class wxConfigBase;

namespace icu::settings {

// 应用设置的统一入口（单例）
// 封装 wxConfig，提供类型安全的读写
// 是项目中唯一允许直接操作 wxConfigBase 的类
class Settings {
public:
    static Settings& Instance();

    void Init();      // 在 App::OnInit 最开始调用
    void Shutdown();  // 在 App::OnExit 调用

    // ---------- 类型化读写 ----------
    wxString GetString(const wxString& key, const wxString& def = "") const;
    void SetString(const wxString& key, const wxString& value);

    int GetInt(const wxString& key, int def = 0) const;
    void SetInt(const wxString& key, int value);

    bool GetBool(const wxString& key, bool def = false) const;
    void SetBool(const wxString& key, bool value);

    // ---------- 管理 ----------
    void DeleteKey(const wxString& key);
    void DeleteGroup(const wxString& group);
    void Flush();

    // ---------- 底层访问（仅框架内部使用） ----------
    wxConfigBase* Raw() const;

private:
    Settings() = default;
};

}  // namespace icu::settings
