#include "ModbusWriteStep.h"

#include "log/Log.h"

namespace icu {

ModbusWriteStep::ModbusWriteStep(const nlohmann::json& params) {
    m_name = params.value("name", "Modbus Write");
    m_variableName = params.value("variable", "");
    m_writeValue =
        params.value("value", "");  // 如果 json 里指定了 value 就写入这个，否则用变量的当前值
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

    // 决定要写入的值：优先取 step 里的 value，否则取变量的当前值
    std::string valStr = m_writeValue.empty() ? var.currentValue : m_writeValue;
    if (valStr.empty()) {
        result.errorMessage = "No value to write.";
        return result;
    }

    try {
        req.writeValues.push_back(std::stoi(valStr));
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
    result.rawResponse = valStr;
    return result;
}

bool ModbusWriteStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    // 写操作默认成功
    return true;
}

void ModbusWriteStep::Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) {
    // 写入之后，同步更新变量池
    if (!result.rawResponse.empty() && vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = result.rawResponse;
    }
}

}  // namespace icu
