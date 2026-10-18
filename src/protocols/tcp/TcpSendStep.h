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

    // ★ 修改为接收 std::map<std::string, CommVariable>&
    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;
    void Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) override;

private:
    std::string m_name;
    std::string m_variableName;  // ★ 新增变量名
    int m_waitMs;
    std::string m_regexPattern;
};

}  // namespace icu
