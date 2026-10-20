#include "MqttProtocol.h"

#include "MqttPublishStep.h"
#include "MqttSession.h"

namespace icu {

namespace {
std::string GetStr(const std::map<std::string, FieldValue>& f, const char* key,
                   const std::string& def = "") {
    auto it = f.find(key);
    return it == f.end() ? def : it->second.AsString();
}

int GetInt(const std::map<std::string, FieldValue>& f, const char* key, int def = 0) {
    auto it = f.find(key);
    if (it == f.end()) return def;
    if (it->second.GetType() == FieldValue::Type::Int) return it->second.AsInt();
    try {
        return std::stoi(it->second.AsString());
    } catch (...) {
        return def;
    }
}
}  // namespace

std::vector<FieldDescriptor> MqttProtocol::FieldsForMode(int /*modeIndex*/) const {
    return {{"brokerHost", wxTRANSLATE("Broker Host:"), FieldValue::Type::String,
             FieldValue(std::string("127.0.0.1"))},
            {"brokerPort", wxTRANSLATE("Broker Port:"), FieldValue::Type::Int, FieldValue(1883), 1,
             65535},
            {"clientId", wxTRANSLATE("Client ID:"), FieldValue::Type::String,
             FieldValue(std::string("IndComm_Client"))},
            {"keepAlive", wxTRANSLATE("Keep Alive (s):"), FieldValue::Type::Int, FieldValue(60), 5,
             3600}};
}

std::vector<FieldDescriptor> MqttProtocol::VariableParamsFields() const {
    return {{"topic", wxTRANSLATE("Topic:"), FieldValue::Type::String,
             FieldValue(std::string("test/topic"))},
            {"qos",
             wxTRANSLATE("QoS:"),
             FieldValue::Type::Choice,
             FieldValue(std::string("0")),
             0,
             0,
             {{"0", "0"}, {"1", "1"}, {"2", "2"}}}};
}

ISessionPtr MqttProtocol::CreateSession(int /*modeIndex*/,
                                        const std::map<std::string, FieldValue>& fields) {
    MqttConfig cfg;
    cfg.brokerHost = GetStr(fields, "brokerHost", "127.0.0.1");
    cfg.brokerPort = GetInt(fields, "brokerPort", 1883);
    cfg.clientId = GetStr(fields, "clientId", "IndComm_Client");
    cfg.keepAlive = GetInt(fields, "keepAlive", 60);

    return std::make_unique<MqttSession>(cfg);
}

std::string MqttProtocol::MakeDefaultFileName(
    int /*modeIndex*/, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    std::string h = sanitize(GetStr(fields, "brokerHost", "127.0.0.1"));
    return "MQTT_" + h + "_" + std::to_string(GetInt(fields, "brokerPort", 1883)) + ".json";
}

ITestStepPtr MqttProtocol::CreateTestStep(const std::string& stepType,
                                          const nlohmann::json& params) {
    if (stepType == "write" || stepType == "send" || stepType == "read") {
        return std::make_unique<MqttPublishStep>(params);
    }
    return nullptr;
}

}  // namespace icu
