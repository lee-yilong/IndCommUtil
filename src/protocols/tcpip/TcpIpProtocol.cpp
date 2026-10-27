#include "TcpIpProtocol.h"

#include "TcpSendStep.h"
#include "TcpTransport.h"
#include "UdpTransport.h"
#include "core/ISession.h"
#include "core/ReconnectConfig.h"
#include "core/StreamSession.h"
#include "protocols/codec/RawCodec.h"

namespace icu {

namespace {
std::string GetStr(const std::map<std::string, FieldValue>& f, const char* key,
                   const std::string& def = "") {
    auto it = f.find(key);
    return it == f.end() ? def : it->second.AsString();
}

int GetInt(const std::map<std::string, FieldValue>& f, const char* key, int def = 0) {
    auto it = f.find(key);
    return it == f.end() ? def : it->second.AsInt();
}

bool GetBool(const std::map<std::string, FieldValue>& f, const char* key, bool def = false) {
    auto it = f.find(key);
    return it == f.end() ? def : it->second.AsBool();
}
}  // namespace

std::vector<std::string> TcpIpProtocol::ModeNames() const {
    return {wxTRANSLATE("TCP"), wxTRANSLATE("UDP")};
}

std::vector<FieldDescriptor> TcpIpProtocol::TcpFields() const {
    std::vector<FieldDescriptor> f = {
        {"role",
         wxTRANSLATE("Role:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("Client")),
         0,
         0,
         {{"Client", wxTRANSLATE("Client")}, {"Server", wxTRANSLATE("Server")}}},
        {"host", wxTRANSLATE("Host / Listen IP:"), FieldValue::Type::String,
         FieldValue(std::string("127.0.0.1"))},
        {"port", wxTRANSLATE("Port:"), FieldValue::Type::Int, FieldValue(502), 1, 65535},
        {"timeoutMs", wxTRANSLATE("Timeout (ms):"), FieldValue::Type::Int, FieldValue(3000), 100,
         600000},
        {"maxClients", wxTRANSLATE("Max Connections:"), FieldValue::Type::Int, FieldValue(10), 1,
         1000}};

    auto reconn = ReconnectConfig::GetFieldDescriptors();
    f.insert(f.end(), reconn.begin(), reconn.end());
    return f;
}

std::vector<FieldDescriptor> TcpIpProtocol::UdpFields() const {
    return {{"role",
             wxTRANSLATE("Role:"),
             FieldValue::Type::Choice,
             FieldValue(std::string("Client")),
             0,
             0,
             {{"Client", wxTRANSLATE("Client")}, {"Server", wxTRANSLATE("Server")}}},
            {"host", wxTRANSLATE("Host / Listen IP:"), FieldValue::Type::String,
             FieldValue(std::string("127.0.0.1"))},
            {"port", wxTRANSLATE("Port:"), FieldValue::Type::Int, FieldValue(5020), 1, 65535},
            {"localPort", wxTRANSLATE("Local Port (0 = auto):"), FieldValue::Type::Int,
             FieldValue(0), 0, 65535}};
}

std::vector<FieldDescriptor> TcpIpProtocol::FieldsForMode(int modeIndex) const {
    return (modeIndex == 1) ? UdpFields() : TcpFields();
}

std::vector<FieldDescriptor> TcpIpProtocol::VariableParamsFields() const {
    return {};
}

ISessionPtr TcpIpProtocol::CreateSession(int modeIndex,
                                         const std::map<std::string, FieldValue>& fields) {
    std::string role = GetStr(fields, "role", "Client");

    if (fields.find("role") == fields.end()) {
        if (fields.find("listenHost") != fields.end()) {
            role = "Server";
        } else {
            role = "Client";
        }
    }

    if (modeIndex == 0) {  // TCP
        std::string host =
            GetStr(fields, "host",
                   GetStr(fields, "remoteHost", GetStr(fields, "listenHost", "127.0.0.1")));
        int port =
            GetInt(fields, "port", GetInt(fields, "remotePort", GetInt(fields, "listenPort", 502)));

        if (role == "Server") {
            TcpServerConfig cfg;
            cfg.listenHost = host;
            cfg.listenPort = port;
            cfg.maxClients = GetInt(fields, "maxClients", 10);
            auto transport = std::make_unique<TcpServerTransport>(cfg);
            return std::make_unique<StreamSession>(std::move(transport),
                                                   std::make_unique<RawCodec>());
        } else {
            TcpClientConfig cfg;
            cfg.remoteHost = host;
            cfg.remotePort = port;
            cfg.connectTimeoutMs = GetInt(fields, "timeoutMs", 3000);
            auto reconn = ReconnectConfig::FromFields(fields);
            cfg.autoReconnect = reconn.autoReconnect;
            cfg.reconnectMaxAttempts = reconn.maxAttempts;
            cfg.reconnectDelaySec = reconn.delaySec;
            auto transport = std::make_unique<TcpClientTransport>(cfg);
            return std::make_unique<StreamSession>(std::move(transport),
                                                   std::make_unique<RawCodec>());
        }
    } else {  // UDP
        UdpConfig cfg;
        cfg.isServer = (role == "Server");
        cfg.host = GetStr(fields, "host", "127.0.0.1");
        cfg.port = GetInt(fields, "port", 5020);
        cfg.localPort = GetInt(fields, "localPort", 0);
        auto transport = std::make_unique<UdpTransport>(cfg);
        return std::make_unique<StreamSession>(std::move(transport), std::make_unique<RawCodec>());
    }
}

std::string TcpIpProtocol::MakeDefaultFileName(
    int modeIndex, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    std::string mode = (modeIndex == 1) ? "UDP" : "TCP";
    std::string role = GetStr(fields, "role", "Client");
    std::string host = GetStr(fields, "host", "127.0.0.1");
    int port = GetInt(fields, "port", (modeIndex == 1) ? 5020 : 502);

    return mode + "_" + role + "_" + sanitize(host) + "_" + std::to_string(port) + ".json";
}

ITestStepPtr TcpIpProtocol::CreateTestStep(const std::string& stepType,
                                           const nlohmann::json& params) {
    if (stepType == "write" || stepType == "send" || stepType == "read") {
        return std::make_unique<TcpSendStep>(params);
    }
    return nullptr;
}

}  // namespace icu
