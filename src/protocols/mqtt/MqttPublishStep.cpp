#include "MqttPublishStep.h"

#include "MqttSession.h"
#include "core/ValueValidator.h"

namespace icu {

MqttPublishStep::MqttPublishStep(const nlohmann::json& params) {
    m_name = params.value("name", "MQTT Step");
    m_variableName = params.value("variable", "");
    m_isReadOnly = (params.value("action", "") == "read");
    m_waitMs = params.value("wait_ms", 0);
    if (params.contains("value")) {
        const auto& value = params["value"];
        if (value.is_string())
            m_writeValue = value.get<std::string>();
        else if (value.is_number())
            m_writeValue = value.dump();
    }
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
        // 发布值优先使用步骤配置，未配置时才回退到变量当前值
        const std::string payload = m_writeValue.empty() ? var.currentValue : m_writeValue;
        std::string validationError;
        if (!ValueValidator::Validate(var.type, payload, validationError)) {
            result.errorMessage = "Invalid value for " + var.type + ": " + validationError;
            return result;
        }
        std::string topic = var.params.value("topic", "test/topic");
        int qos = var.params.value("qos", 0);
        mqttSession->Publish(topic, payload, qos);
        result.isSendAction = true;
        result.rawResponse = payload;
    } else {
        result.rawResponse = var.currentValue;
    }

    result.success = true;
    return result;
}

bool MqttPublishStep::Assert(const TestResult& /*result*/, std::string& /*outErrorMsg*/) {
    return true;
}

std::vector<std::string> MqttPublishStep::Extract(const TestResult& result,
                                                  std::map<std::string, CommVariable>& vars) {
    std::vector<std::string> updated;
    if (!result.rawResponse.empty() && vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = result.rawResponse;
        updated.push_back(m_variableName);
    }
    return updated;
}

}  // namespace icu
