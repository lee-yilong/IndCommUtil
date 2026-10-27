#pragma once

#include <nlohmann/json.hpp>
#include <regex>

#include "core/ITestStep.h"
#include "core/StreamSession.h"

namespace icu {

class TcpSendStep : public ITestStep {
public:
    TcpSendStep(const nlohmann::json& params);
    std::string Name() const override;

    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;
    std::vector<std::string> Extract(const TestResult& result,
                                     std::map<std::string, CommVariable>& vars) override;
    // 读步骤在 Execute 内部等待响应；写步骤由 Runner 在步骤后等待配置时间
    int WaitMs() const override { return m_isReadOnly ? 0 : m_waitMs; }

private:
    std::string m_name;
    std::string m_variableName;
    std::string m_writeValue;
    int m_waitMs;
    bool m_isReadOnly = false;

    std::string m_assertType;
    std::string m_expectedValue;
};

}  // namespace icu
