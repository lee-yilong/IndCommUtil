#pragma once

namespace icu::settings {

enum class Theme {
    System,
    Light,
    Dark,
};

class ThemeSettings {
public:
    static ThemeSettings& Instance();

    Theme GetTheme() const;
    void SetTheme(Theme);
    void Apply();

private:
    ThemeSettings() = default;
};

}  // namespace icu::settings
