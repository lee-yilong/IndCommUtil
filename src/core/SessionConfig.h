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
    nlohmann::json params;     // 协议专属参数，如 Modbus 的 slaveId, functionCode 等
    std::string currentValue;  // 运行时提取的值
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
    std::map<std::string, CommVariable> variables;  // ★ 变量池
    ManualModeConfig manualMode;
    InteractiveModeConfig interactiveMode;
    AutoModeConfig autoMode;
};

}  // namespace icu
