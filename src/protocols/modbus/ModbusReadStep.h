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

    // ★ 修改为接收 std::map<std::string, CommVariable>&
    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;
    void Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) override;

private:
    std::string m_name;
    std::string m_variableName;  // ★ 新增变量名

    int m_expectedValue;
    bool m_hasAssert;
};

}  // namespace icu
