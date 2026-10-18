#pragma once

#include <nlohmann/json.hpp>

#include "core/ITestStep.h"
#include "core/ITransactionSession.h"

namespace icu {

class ModbusWriteStep : public ITestStep {
public:
    explicit ModbusWriteStep(const nlohmann::json& params);
    ~ModbusWriteStep() override = default;

    std::string Name() const override;
    TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) override;
    bool Assert(const TestResult& result, std::string& outErrorMsg) override;
    void Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) override;

private:
    std::string m_name;
    std::string m_variableName;
    std::string m_writeValue;
};

}  // namespace icu
