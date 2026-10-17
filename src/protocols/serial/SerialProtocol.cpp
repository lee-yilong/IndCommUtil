#include "SerialProtocol.h"

#include "core/ISession.h"
#include "core/StreamSession.h"
#include "protocols/codec/RawCodec.h"
#include "protocols/serial/SerialTransport.h"

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
    const auto t = it->second.GetType();
    if (t == FieldValue::Type::Int) return it->second.AsInt();
    if (t == FieldValue::Type::String || t == FieldValue::Type::Choice) {
        try {
            return std::stoi(it->second.AsString());
        } catch (...) {
            return def;
        }
    }
    return def;
}
SerialPort::Parity ParseParity(const std::string& s) {
    if (s == "E") return SerialPort::Parity::Even;
    if (s == "O") return SerialPort::Parity::Odd;
    if (s == "M") return SerialPort::Parity::Mark;
    if (s == "S") return SerialPort::Parity::Space;
    return SerialPort::Parity::None;
}
SerialPort::StopBits ParseStopBits(const std::string& s) {
    if (s == "1.5") return SerialPort::StopBits::OnePointFive;
    if (s == "2") return SerialPort::StopBits::Two;
    return SerialPort::StopBits::One;
}
SerialPort::FlowControl ParseFlow(const std::string& s) {
    if (s == "RTS/CTS") return SerialPort::FlowControl::RtsCts;
    if (s == "XON/XOFF") return SerialPort::FlowControl::XonXoff;
    return SerialPort::FlowControl::None;
}
}  // namespace

std::vector<FieldDescriptor> SerialProtocol::FieldsForMode(int /*modeIndex*/) const {
    auto ports = SerialPort::EnumeratePorts();
    std::vector<std::pair<std::string, std::string>> portChoices;
    for (const auto& p : ports) portChoices.emplace_back(p, p);
    if (portChoices.empty()) portChoices.emplace_back("COM1", "COM1");
    const std::string defaultPort = portChoices.front().first;

    return {
        {"portName", wxTRANSLATE("Port:"), FieldValue::Type::Choice, FieldValue(defaultPort), 0, 0,
         portChoices},
        {"baudRate",
         wxTRANSLATE("Baud Rate:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("9600")),
         0,
         0,
         {{"1200", "1200"},
          {"2400", "2400"},
          {"4800", "4800"},
          {"9600", "9600"},
          {"19200", "19200"},
          {"38400", "38400"},
          {"57600", "57600"},
          {"115200", "115200"},
          {"230400", "230400"},
          {"460800", "460800"},
          {"921600", "921600"}}},
        {"dataBits",
         wxTRANSLATE("Data Bits:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("8")),
         0,
         0,
         {{"5", "5"}, {"6", "6"}, {"7", "7"}, {"8", "8"}}},
        {"parity",
         wxTRANSLATE("Parity:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("N")),
         0,
         0,
         {{"N", wxTRANSLATE("None")},
          {"E", wxTRANSLATE("Even")},
          {"O", wxTRANSLATE("Odd")},
          {"M", wxTRANSLATE("Mark")},
          {"S", wxTRANSLATE("Space")}}},
        {"stopBits",
         wxTRANSLATE("Stop Bits:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("1")),
         0,
         0,
         {{"1", "1"}, {"1.5", "1.5"}, {"2", "2"}}},
        {"flowControl",
         wxTRANSLATE("Flow Control:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("None")),
         0,
         0,
         {{"None", wxTRANSLATE("None")},
          {"RTS/CTS", wxTRANSLATE("RTS/CTS")},
          {"XON/XOFF", wxTRANSLATE("XON/XOFF")}}},
        {"readTimeoutMs", wxTRANSLATE("Read Timeout (ms):"), FieldValue::Type::Int, FieldValue(100),
         10, 60000},
        {"writeTimeoutMs", wxTRANSLATE("Write Timeout (ms):"), FieldValue::Type::Int,
         FieldValue(1000), 10, 60000},
    };
}

ISessionPtr SerialProtocol::CreateSession(int /*modeIndex*/,
                                          const std::map<std::string, FieldValue>& fields) {
    SerialPort::Config cfg;
    cfg.portName = GetStr(fields, "portName", "COM1");
    cfg.baudRate = GetInt(fields, "baudRate", 9600);
    cfg.dataBits = GetInt(fields, "dataBits", 8);
    cfg.parity = ParseParity(GetStr(fields, "parity", "N"));
    cfg.stopBits = ParseStopBits(GetStr(fields, "stopBits", "1"));
    cfg.flowControl = ParseFlow(GetStr(fields, "flowControl", "None"));
    cfg.readTimeoutMs = GetInt(fields, "readTimeoutMs", 100);
    cfg.writeTimeoutMs = GetInt(fields, "writeTimeoutMs", 1000);

    auto transport = std::make_unique<SerialTransport>(cfg);
    auto codec = std::make_unique<RawCodec>();
    return std::make_unique<StreamSession>(std::move(transport), std::move(codec));
}

std::string SerialProtocol::MakeDefaultFileName(
    int /*modeIndex*/, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    const std::string port = sanitize(GetStr(fields, "portName", "COM1"));
    const std::string baud = GetStr(fields, "baudRate", "9600");
    return "Serial_" + port + "_" + baud + ".json";
}

}  // namespace icu
