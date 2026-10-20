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

private:
    std::string m_name;
    std::string m_variableName;
    int m_waitMs;
    bool m_isReadOnly = false;

    // ★ 断言控制
    std::string m_assertType;
    std::string m_expectedValue;
};

}  // namespace icu
