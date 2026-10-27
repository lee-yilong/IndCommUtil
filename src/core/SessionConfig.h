#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace icu {

enum class SessionMode { Manual, Interactive, Auto };

// 单个通信变量定义
struct CommVariable {
    std::string name;          // 变量名，如 "temp_val"
    std::string type;          // 类型，如 "int", "string", "bool"
    std::string access;        // "RO" (只读), "RW" (读写), "WO" (只写)
    nlohmann::json params;     // 协议专属参数
    std::string currentValue;  // 运行时提取的值
    std::string rawBytes;      // 原始字节的十六进制字符串
    bool enabled = true;       // ★ 新增：是否启用
};

struct ManualModeConfig {
    std::string sendPayload;
    bool appendNewline = true;
};

struct InteractiveModeConfig {
    int currentStepIndex = 0;
    int startStepIndex = 0;
    int endStepIndex = 0;
    nlohmann::json testFlow = nlohmann::json::array();
};

struct AutoModeConfig {
    int startStepIndex = 0;
    int endStepIndex = 0;
    nlohmann::json testFlow = nlohmann::json::array();
};

struct SessionConfig {
    SessionMode activeMode = SessionMode::Manual;
    std::map<std::string, CommVariable> variables;
    // 变量在 UI 中的显示顺序（仅保存已有变量；缺失的会按名称追加）
    std::vector<std::string> variableOrder;
    ManualModeConfig manualMode;
    InteractiveModeConfig interactiveMode;
    AutoModeConfig autoMode;
};

}  // namespace icu
