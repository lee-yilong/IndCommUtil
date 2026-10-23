#include "MqttProtocol.h"

#include "MqttBrokerSession.h"
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

std::vector<std::string> MqttProtocol::ModeNames() const {
    return {wxTRANSLATE("Publisher"), wxTRANSLATE("Subscriber"), wxTRANSLATE("Broker")};
}

std::vector<FieldDescriptor> MqttProtocol::ClientFields() const {
    return {{"brokerHost", wxTRANSLATE("Broker Host:"), FieldValue::Type::String,
             FieldValue(std::string("127.0.0.1"))},
            {"brokerPort", wxTRANSLATE("Broker Port:"), FieldValue::Type::Int, FieldValue(1883), 1,
             65535},
            {"clientId", wxTRANSLATE("Client ID:"), FieldValue::Type::String,
             FieldValue(std::string("IndComm_Pub"))},
            {"keepAlive", wxTRANSLATE("Keep Alive (s):"), FieldValue::Type::Int, FieldValue(60), 5,
             3600}};
}

std::vector<FieldDescriptor> MqttProtocol::SubscriberFields() const {
    auto f = ClientFields();
    // 替换默认 Client ID
    f[2].defaultValue = FieldValue(std::string("IndComm_Sub"));

    f.push_back({"topic", wxTRANSLATE("Subscribe Topic:"), FieldValue::Type::String,
                 FieldValue(std::string("#"))});
    f.push_back({"qos",
                 wxTRANSLATE("Subscribe QoS:"),
                 FieldValue::Type::Choice,
                 FieldValue(std::string("0")),
                 0,
                 0,
                 {{"0", "0"}, {"1", "1"}, {"2", "2"}}});
    return f;
}

std::vector<FieldDescriptor> MqttProtocol::BrokerFields() const {
    return {{"listenPort", wxTRANSLATE("Listen Port:"), FieldValue::Type::Int, FieldValue(1883), 1,
             65535}};
}

std::vector<FieldDescriptor> MqttProtocol::FieldsForMode(int modeIndex) const {
    switch (modeIndex) {
        case 1:
            return SubscriberFields();
        case 2:
            return BrokerFields();
        default:
            return ClientFields();
    }
}

std::vector<FieldDescriptor> MqttProtocol::VariableParamsFields() const {
    return {{"topic", wxTRANSLATE("Publish Topic:"), FieldValue::Type::String,
             FieldValue(std::string("test/topic"))},
            {"qos",
             wxTRANSLATE("Publish QoS:"),
             FieldValue::Type::Choice,
             FieldValue(std::string("0")),
             0,
             0,
             {{"0", "0"}, {"1", "1"}, {"2", "2"}}}};
}

ISessionPtr MqttProtocol::CreateSession(int modeIndex,
                                        const std::map<std::string, FieldValue>& fields) {
    if (modeIndex == 2) {
        MqttBrokerConfig cfg;
        cfg.listenPort = GetInt(fields, "listenPort", 1883);
        return std::make_unique<MqttBrokerSession>(cfg);
    }

    MqttConfig cfg;
    cfg.brokerHost = GetStr(fields, "brokerHost", "127.0.0.1");
    cfg.brokerPort = GetInt(fields, "brokerPort", 1883);
    cfg.clientId = GetStr(fields, "clientId", "IndComm_Client");
    cfg.keepAlive = GetInt(fields, "keepAlive", 60);

    cfg.isSubscriber = (modeIndex == 1);
    if (cfg.isSubscriber) {
        cfg.subscribeTopic = GetStr(fields, "topic", "#");
        cfg.subscribeQos = GetInt(fields, "qos", 0);
    }

    return std::make_unique<MqttSession>(cfg);
}

std::string MqttProtocol::MakeDefaultFileName(
    int modeIndex, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    if (modeIndex == 2) {
        return "MQTT_Broker_" + std::to_string(GetInt(fields, "listenPort", 1883)) + ".json";
    }

    std::string modeName = (modeIndex == 1) ? "Subscriber" : "Publisher";
    std::string h = sanitize(GetStr(fields, "brokerHost", "127.0.0.1"));
    return "MQTT_" + modeName + "_" + h + "_" + std::to_string(GetInt(fields, "brokerPort", 1883)) +
           ".json";
}

ITestStepPtr MqttProtocol::CreateTestStep(const std::string& stepType,
                                          const nlohmann::json& params) {
    if (stepType == "write" || stepType == "send" || stepType == "read") {
        return std::make_unique<MqttPublishStep>(params);
    }
    return nullptr;
}

}  // namespace icu
