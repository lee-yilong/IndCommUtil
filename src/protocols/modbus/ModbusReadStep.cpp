#include "ModbusReadStep.h"

#include "log/Log.h"

namespace icu {

ModbusReadStep::ModbusReadStep(const nlohmann::json& params) {
    m_name = params.value("name", "Modbus Read");
    m_variableName = params.value("variable", "");

    m_hasAssert = false;
    if (params.contains("assert") && params["assert"].is_object()) {
        const auto& a = params["assert"];
        if (a.contains("expected")) {
            if (a["expected"].is_number())
                m_expectedValue = a["expected"].get<int>();
            else if (a["expected"].is_string()) {
                try {
                    m_expectedValue = std::stoi(a["expected"].get<std::string>());
                } catch (...) {
                }
            }
            m_hasAssert = true;
        }
    }
}

std::string ModbusReadStep::Name() const {
    return m_name;
}

TestResult ModbusReadStep::Execute(ISession* session, std::map<std::string, CommVariable>& vars) {
    TestResult result;
    auto txSession = dynamic_cast<ITransactionSession*>(session);
    if (!txSession) {
        result.errorMessage = "Not a transaction session";
        return result;
    }

    if (vars.find(m_variableName) == vars.end()) {
        result.errorMessage = "Variable not found: " + m_variableName;
        return result;
    }
    auto& var = vars[m_variableName];

    if (var.access != "RW" && var.access != "RO") {
        result.errorMessage = "Variable is WO, cannot read.";
        return result;
    }

    TransactionRequest req;
    req.slaveId = var.params.value("slaveId", 1);
    req.functionCode = var.params.value("functionCode", 3);
    req.startAddress = var.params.value("startAddress", 0);
    req.count = var.params.value("count", 1);

    TransactionResponse resp = txSession->Execute(req);
    if (!resp.ok) {
        result.success = false;
        result.errorMessage = resp.errorMessage;
        return result;
    }

    result.success = true;
    result.registers = resp.registers;
    return result;
}

bool ModbusReadStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    if (!m_hasAssert) return true;
    if (result.registers.empty()) {
        outErrorMsg = "No registers returned";
        return false;
    }

    int actualValue = result.registers[0];
    if (actualValue != m_expectedValue) {
        outErrorMsg =
            "Expected " + std::to_string(m_expectedValue) + ", got " + std::to_string(actualValue);
        return false;
    }
    return true;
}

// ★ 修改返回值
std::vector<std::string> ModbusReadStep::Extract(const TestResult& result,
                                                 std::map<std::string, CommVariable>& vars) {
    std::vector<std::string> updated;
    if (!result.registers.empty() && vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = std::to_string(result.registers[0]);
        updated.push_back(m_variableName);
    }
    return updated;
}

}  // namespace icu
