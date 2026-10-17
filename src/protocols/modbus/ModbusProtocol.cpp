#include "ModbusProtocol.h"

#include "ModbusSession.h"
#include "core/ReconnectConfig.h"

namespace icu {

namespace {

std::string GetStr(const std::map<std::string, FieldValue>& f, const char* key,
                   const std::string& def = "") {
    auto it = f.find(key);
    return it == f.end() ? def : it->second.AsString();
}

int GetInt(const std::map<std::string, FieldValue>& f, const char* key, int def = 0) {
    auto it = f.find(key);
    if (it == f.end()) return def;
    if (it->second.GetType() == FieldValue::Type::Int) return it->second.AsInt();
    try {
        return std::stoi(it->second.AsString());
    } catch (...) {
        return def;
    }
}

std::vector<FieldDescriptor> CommonFields() {
    std::vector<FieldDescriptor> f = {
        {"slaveId", wxTRANSLATE("Slave ID:"), FieldValue::Type::Int, FieldValue(1), 0, 247},
        {"timeoutMs", wxTRANSLATE("Timeout (ms):"), FieldValue::Type::Int, FieldValue(1000), 100,
         60000}};

    // 插入标准重连字段
    auto reconn = ReconnectConfig::GetFieldDescriptors();
    f.insert(f.end(), reconn.begin(), reconn.end());
    return f;
}

}  // namespace

std::vector<std::string> ModbusProtocol::ModeNames() const {
    return {wxTRANSLATE("TCP"), wxTRANSLATE("RTU"), wxTRANSLATE("ASCII")};
}

std::vector<FieldDescriptor> ModbusProtocol::TcpFields() const {
    auto f = CommonFields();
    f.push_back({"host", wxTRANSLATE("Host:"), FieldValue::Type::String,
                 FieldValue(std::string("127.0.0.1"))});
    f.push_back({"port", wxTRANSLATE("Port:"), FieldValue::Type::Int, FieldValue(502), 1, 65535});
    f.push_back(
        {"role",
         wxTRANSLATE("Role:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("Client")),
         0,
         0,
         {{"Client", wxTRANSLATE("Client (Master)")}, {"Server", wxTRANSLATE("Server (Slave)")}}});
    return f;
}

std::vector<FieldDescriptor> ModbusProtocol::RtuFields() const {
    auto f = CommonFields();
    f.push_back({"portName", wxTRANSLATE("Port:"), FieldValue::Type::String,
                 FieldValue(std::string("COM3"))});
    f.push_back({"baudRate",
                 wxTRANSLATE("Baud Rate:"),
                 FieldValue::Type::Choice,
                 FieldValue(std::string("9600")),
                 0,
                 0,
                 {{"4800", "4800"},
                  {"9600", "9600"},
                  {"19200", "19200"},
                  {"38400", "38400"},
                  {"57600", "57600"},
                  {"115200", "115200"}}});
    f.push_back(
        {"parity",
         wxTRANSLATE("Parity:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("N")),
         0,
         0,
         {{"N", wxTRANSLATE("None")}, {"E", wxTRANSLATE("Even")}, {"O", wxTRANSLATE("Odd")}}});
    f.push_back(
        {"role",
         wxTRANSLATE("Role:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("Client")),
         0,
         0,
         {{"Client", wxTRANSLATE("Client (Master)")}, {"Server", wxTRANSLATE("Server (Slave)")}}});
    return f;
}

std::vector<FieldDescriptor> ModbusProtocol::AsciiFields() const {
    return RtuFields();
}

std::vector<FieldDescriptor> ModbusProtocol::FieldsForMode(int modeIndex) const {
    switch (modeIndex) {
        case 1:
            return RtuFields();
        case 2:
            return AsciiFields();
        default:
            return TcpFields();
    }
}

std::vector<FieldDescriptor> ModbusProtocol::OperationFieldsForMode(int /*modeIndex*/) const {
    return {
        {"slaveId", wxTRANSLATE("Slave ID:"), FieldValue::Type::Int, FieldValue(1), 0, 247},
        {"functionCode",
         wxTRANSLATE("Function Code:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("3")),
         0,
         0,
         {{"1", "01 - Read Coils"},
          {"2", "02 - Read Discrete Inputs"},
          {"3", "03 - Read Holding Registers"},
          {"4", "04 - Read Input Registers"},
          {"5", "05 - Write Single Coil"},
          {"6", "06 - Write Single Register"},
          {"15", "15 - Write Multiple Coils"},
          {"16", "16 - Write Multiple Registers"}}},
        {"startAddress", wxTRANSLATE("Start Address:"), FieldValue::Type::Int, FieldValue(0), 0,
         65535},
        {"count", wxTRANSLATE("Count:"), FieldValue::Type::Int, FieldValue(10), 1, 2000},
        {"writeValues", wxTRANSLATE("Write Values (comma sep):"), FieldValue::Type::String,
         FieldValue(std::string(""))},
    };
}

ISessionPtr ModbusProtocol::CreateSession(int modeIndex,
                                          const std::map<std::string, FieldValue>& fields) {
    ModbusConfig cfg;
    cfg.modeIndex = modeIndex;
    cfg.slaveId = GetInt(fields, "slaveId", 1);
    cfg.timeoutMs = GetInt(fields, "timeoutMs", 1000);
    cfg.role = GetStr(fields, "role", "Client");

    // 统一解析重连配置
    cfg.reconn = ReconnectConfig::FromFields(fields);

    if (modeIndex == 0) {
        cfg.host = GetStr(fields, "host", "127.0.0.1");
        cfg.port = GetInt(fields, "port", 502);
    } else {
        cfg.portName = GetStr(fields, "portName", "COM3");
        cfg.baudRate = GetInt(fields, "baudRate", 9600);
        cfg.parity = GetStr(fields, "parity", "N");
    }

    return std::make_unique<ModbusSession>(cfg);
}

std::string ModbusProtocol::MakeDefaultFileName(
    int modeIndex, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    const std::string role = GetStr(fields, "role", "Client");
    if (modeIndex == 0) {
        return "ModbusTCP_" + role + "_" + sanitize(GetStr(fields, "host", "127.0.0.1")) + "_" +
               std::to_string(GetInt(fields, "port", 502)) + ".json";
    }
    return "ModbusRTU_" + role + "_" + sanitize(GetStr(fields, "portName", "COM3")) + "_" +
           std::to_string(GetInt(fields, "baudRate", 9600)) + ".json";
}

}  // namespace icu
