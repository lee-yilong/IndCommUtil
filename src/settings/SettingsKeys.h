#pragma once

namespace icu::settings {

// =========================================================================
// 语言
// =========================================================================
constexpr const char* kLanguage = "/Settings/Language";

// =========================================================================
// 窗口几何
// =========================================================================
constexpr const char* kMainWindowX = "/Window/Main/X";
constexpr const char* kMainWindowY = "/Window/Main/Y";
constexpr const char* kMainWindowW = "/Window/Main/Width";
constexpr const char* kMainWindowH = "/Window/Main/Height";
constexpr const char* kMainWindowMaximized = "/Window/Main/Maximized";

constexpr const char* kLogWindowX = "/Window/Log/X";
constexpr const char* kLogWindowY = "/Window/Log/Y";
constexpr const char* kLogWindowW = "/Window/Log/Width";
constexpr const char* kLogWindowH = "/Window/Log/Height";

// =========================================================================
// UI 外观与行为
// =========================================================================
constexpr const char* kUiFontSize = "/UI/FontSize";
constexpr const char* kUiHexDisplay = "/UI/HexDisplay";
constexpr const char* kUiAutoScroll = "/UI/AutoScroll";
constexpr const char* kUiShowTimestamp = "/UI/ShowTimestamp";
constexpr const char* kUiTheme = "/UI/Theme";
constexpr const char* kMinimizeToTrayOnClose = "/Settings/MinimizeToTrayOnClose";

// =========================================================================
// 字体（预留）
// =========================================================================
constexpr const char* kFontUiFace = "/Settings/Font/UI/Face";
constexpr const char* kFontUiSize = "/Settings/Font/UI/Size";
constexpr const char* kFontLogFace = "/Settings/Font/Log/Face";
constexpr const char* kFontLogSize = "/Settings/Font/Log/Size";
constexpr const char* kFontDataFace = "/Settings/Font/Data/Face";
constexpr const char* kFontDataSize = "/Settings/Font/Data/Size";

// =========================================================================
// 图标（预留）
// =========================================================================
constexpr const char* kIconProtocolPrefix = "/Icons/Protocol/";

// =========================================================================
// 最近连接
// =========================================================================
constexpr const char* kRecentConnections = "/RecentConnections";

}  // namespace icu::settings
