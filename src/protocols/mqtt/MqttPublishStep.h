#pragma once

#include <nlohmann/json.hpp>

#include "core/ITestStep.h"

namespace icu {

class MqttPublishStep : public ITestStep {
public:
    explicit MqttPublishStep(const nlohmann::json& params);
    ~MqttPublishStep() override = default;

    std::string Name() const override;
    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;
    std::vector<std::string> Extract(const TestResult& result,
                                     std::map<std::string, CommVariable>& vars) override;
    int WaitMs() const override { return m_waitMs; }

private:
    std::string m_name;
    std::string m_variableName;
    bool m_isReadOnly = false;
    int m_waitMs = 0;
};

}  // namespace icu
