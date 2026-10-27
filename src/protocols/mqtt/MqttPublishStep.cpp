#include "MqttPublishStep.h"

#include "MqttSession.h"
#include "core/ValueValidator.h"

namespace icu {

MqttPublishStep::MqttPublishStep(const nlohmann::json& params) {
    m_name = params.value("name", "MQTT Step");
    m_variableName = params.value("variable", "");
    m_isReadOnly = (params.value("action", "") == "read");
    m_waitMs = params.value("wait_ms", 0);
}

std::string MqttPublishStep::Name() const {
    return m_name;
}

TestResult MqttPublishStep::Execute(ISession* session, std::map<std::string, CommVariable>& vars) {
    TestResult result;
    auto mqttSession = dynamic_cast<MqttSession*>(session);
    if (!mqttSession) {
        result.errorMessage = "Not an MQTT session";
        return result;
    }

    if (vars.find(m_variableName) == vars.end()) {
        result.errorMessage = "Variable not found: " + m_variableName;
        return result;
    }

    auto& var = vars[m_variableName];

    if (!m_isReadOnly) {
        std::string validationError;
        if (!ValueValidator::Validate(var.type, var.currentValue, validationError)) {
            result.errorMessage = "Invalid value for " + var.type + ": " + validationError;
            return result;
        }
        std::string topic = var.params.value("topic", "test/topic");
        int qos = var.params.value("qos", 0);
        mqttSession->Publish(topic, var.currentValue, qos);
    }

    result.success = true;
    result.rawResponse = var.currentValue;
    return result;
}

bool MqttPublishStep::Assert(const TestResult& /*result*/, std::string& /*outErrorMsg*/) {
    return true;
}

std::vector<std::string> MqttPublishStep::Extract(const TestResult& /*result*/,
                                                  std::map<std::string, CommVariable>& /*vars*/) {
    return {};
}

}  // namespace icu
