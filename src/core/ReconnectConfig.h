#pragma once

#include <wx/translation.h>

#include <map>
#include <string>
#include <vector>

#include "core/ProtocolTypes.h"

namespace icu {

struct ReconnectConfig {
    bool autoReconnect = true;
    int maxAttempts = 0;  // 0 = 无限
    int delaySec = 3;     // 重连间隔（秒）

    // 供 FieldDescriptor 生成 UI
    static std::vector<FieldDescriptor> GetFieldDescriptors() {
        return {// 注意这里最后面多了一个 {}, 和分组名
                {"autoReconnect",
                 wxTRANSLATE("Auto reconnect on disconnect"),
                 FieldValue::Type::Bool,
                 FieldValue(true),
                 0,
                 0,
                 {},
                 wxTRANSLATE("Reconnect Settings")},

                {"reconnectMaxAttempts",
                 wxTRANSLATE("Max reconnect attempts:"),
                 FieldValue::Type::Choice,
                 FieldValue(std::string("0")),
                 0,
                 0,
                 {{"0", wxTRANSLATE("Unlimited")},
                  {"1", wxTRANSLATE("1")},
                  {"3", wxTRANSLATE("3")},
                  {"5", wxTRANSLATE("5")},
                  {"10", wxTRANSLATE("10")},
                  {"30", wxTRANSLATE("30")}},
                 wxTRANSLATE("Reconnect Settings")},

                {"reconnectDelaySec",
                 wxTRANSLATE("Reconnect delay:"),
                 FieldValue::Type::Choice,
                 FieldValue(std::string("3")),
                 0,
                 0,
                 {{"1", wxTRANSLATE("1 s")},
                  {"3", wxTRANSLATE("3 s")},
                  {"5", wxTRANSLATE("5 s")},
                  {"10", wxTRANSLATE("10 s")},
                  {"30", wxTRANSLATE("30 s")}},
                 wxTRANSLATE("Reconnect Settings")}};
    }

    // 供 CreateSession 解析字段
    static ReconnectConfig FromFields(const std::map<std::string, FieldValue>& fields) {
        ReconnectConfig cfg;

        auto itAuto = fields.find("autoReconnect");
        if (itAuto != fields.end()) cfg.autoReconnect = itAuto->second.AsBool();

        auto itMax = fields.find("reconnectMaxAttempts");
        if (itMax != fields.end()) {
            if (itMax->second.GetType() == FieldValue::Type::Int)
                cfg.maxAttempts = itMax->second.AsInt();
            else {
                try {
                    cfg.maxAttempts = std::stoi(itMax->second.AsString());
                } catch (...) {
                }
            }
        }

        auto itDelay = fields.find("reconnectDelaySec");
        if (itDelay != fields.end()) {
            if (itDelay->second.GetType() == FieldValue::Type::Int)
                cfg.delaySec = itDelay->second.AsInt();
            else {
                try {
                    cfg.delaySec = std::stoi(itDelay->second.AsString());
                } catch (...) {
                }
            }
        }

        return cfg;
    }
};

}  // namespace icu
