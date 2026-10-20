#pragma once

#include <nlohmann/json.hpp>

#include "core/ITestStep.h"
#include "core/ITransactionSession.h"

namespace icu {

class ModbusReadStep : public ITestStep {
public:
    explicit ModbusReadStep(const nlohmann::json& params);
    ~ModbusReadStep() override = default;

    std::string Name() const override;
    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;

    // ★ 修改返回值
    std::vector<std::string> Extract(const TestResult& result,
                                     std::map<std::string, CommVariable>& vars) override;

private:
    std::string m_name;
    std::string m_variableName;

    int m_expectedValue;
    bool m_hasAssert;
};

}  // namespace icu
