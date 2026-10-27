#include "ModbusWriteStep.h"

#include "core/ValueValidator.h"
#include "log/Log.h"

namespace icu {

ModbusWriteStep::ModbusWriteStep(const nlohmann::json& params) {
    m_name = params.value("name", "Modbus Write");
    m_variableName = params.value("variable", "");
    m_writeValue = params.value("value", "");
    m_waitMs = params.value("wait_ms", 0);
}

std::string ModbusWriteStep::Name() const {
    return m_name;
}

TestResult ModbusWriteStep::Execute(ISession* session, std::map<std::string, CommVariable>& vars) {
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

    if (var.access != "RW" && var.access != "WO") {
        result.errorMessage = "Variable is RO, cannot write.";
        return result;
    }

    TransactionRequest req;
    req.slaveId = var.params.value("slaveId", 1);

    int fc = var.params.value("functionCode", 6);
    if (fc == 1)
        fc = 5;
    else if (fc == 3)
        fc = 6;
    else if (fc == 4)
        fc = 6;

    req.functionCode = fc;
    req.startAddress = var.params.value("startAddress", 0);

    std::string valStr = m_writeValue.empty() ? var.currentValue : m_writeValue;
    if (valStr.empty()) {
        result.errorMessage = "No value to write.";
        return result;
    }

    std::string validationError;
    if (!ValueValidator::Validate(var.type, valStr, validationError)) {
        result.errorMessage = "Invalid value for " + var.type + ": " + validationError;
        return result;
    }

    try {
        const int v = std::stoi(valStr);
        if (v < 0 || v > 65535) {
            result.errorMessage = "Modbus register value must be 0..65535: " + valStr;
            return result;
        }
        req.writeValues.push_back(static_cast<uint16_t>(v));
    } catch (...) {
        result.errorMessage = "Invalid integer value: " + valStr;
        return result;
    }

    auto resp = txSession->Execute(req);
    if (!resp.ok) {
        result.success = false;
        result.errorMessage = resp.errorMessage;
        return result;
    }

    result.success = true;
    result.isSendAction = true;
    result.rawResponse = valStr;
    return result;
}

bool ModbusWriteStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    return true;
}

// ★ 修改返回值
std::vector<std::string> ModbusWriteStep::Extract(const TestResult& result,
                                                  std::map<std::string, CommVariable>& vars) {
    std::vector<std::string> updated;
    if (!result.rawResponse.empty() && vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = result.rawResponse;
        updated.push_back(m_variableName);
    }
    return updated;
}

}  // namespace icu
