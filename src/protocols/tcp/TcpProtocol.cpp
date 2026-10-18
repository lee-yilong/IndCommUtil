#include "TcpProtocol.h"

#include "TcpSendStep.h"
#include "TcpTransport.h"
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

std::vector<std::string> TcpProtocol::ModeNames() const {
    return {wxTRANSLATE("Client"), wxTRANSLATE("Server")};
}

std::vector<FieldDescriptor> TcpProtocol::ClientFields() const {
    std::vector<FieldDescriptor> f = {
        {"remoteHost", wxTRANSLATE("Remote Host:"), FieldValue::Type::String,
         FieldValue(std::string("127.0.0.1"))},
        {"remotePort", wxTRANSLATE("Remote Port:"), FieldValue::Type::Int, FieldValue(502), 1,
         65535},
        {"localHost", wxTRANSLATE("Local Host (optional):"), FieldValue::Type::String,
         FieldValue(std::string("0.0.0.0"))},
        {"localPort", wxTRANSLATE("Local Port (0 = auto):"), FieldValue::Type::Int, FieldValue(0),
         0, 65535},
        {"timeoutMs", wxTRANSLATE("Connect Timeout (ms):"), FieldValue::Type::Int, FieldValue(3000),
         100, 600000}};

    // 插入标准重连字段
    auto reconn = ReconnectConfig::GetFieldDescriptors();
    f.insert(f.end(), reconn.begin(), reconn.end());

    return f;
}

std::vector<FieldDescriptor> TcpProtocol::ServerFields() const {
    return {
        {"listenHost", wxTRANSLATE("Listen Host:"), FieldValue::Type::String,
         FieldValue(std::string("0.0.0.0"))},
        {"listenPort", wxTRANSLATE("Listen Port:"), FieldValue::Type::Int, FieldValue(502), 1,
         65535},
        {"maxClients", wxTRANSLATE("Max Clients:"), FieldValue::Type::Int, FieldValue(10), 1, 1000},
        {"backlog", wxTRANSLATE("Listen Backlog:"), FieldValue::Type::Int, FieldValue(5), 1, 1000},
    };
}

std::vector<FieldDescriptor> TcpProtocol::FieldsForMode(int modeIndex) const {
    return (modeIndex == 1) ? ServerFields() : ClientFields();
}

ISessionPtr TcpProtocol::CreateSession(int modeIndex,
                                       const std::map<std::string, FieldValue>& fields) {
    if (modeIndex == 1) {
        TcpServerConfig cfg;
        cfg.listenHost = GetStr(fields, "listenHost", "0.0.0.0");
        cfg.listenPort = GetInt(fields, "listenPort", 502);
        cfg.maxClients = GetInt(fields, "maxClients", 10);
        cfg.backlog = GetInt(fields, "backlog", 5);
        auto transport = std::make_unique<TcpServerTransport>(cfg);
        auto codec = std::make_unique<RawCodec>();
        return std::make_unique<StreamSession>(std::move(transport), std::move(codec));
    }

    TcpClientConfig cfg;
    cfg.remoteHost = GetStr(fields, "remoteHost", "127.0.0.1");
    cfg.remotePort = GetInt(fields, "remotePort", 502);
    cfg.connectTimeoutMs = GetInt(fields, "timeoutMs", 3000);

    // 统一解析重连配置
    auto reconn = ReconnectConfig::FromFields(fields);
    cfg.autoReconnect = reconn.autoReconnect;
    cfg.reconnectMaxAttempts = reconn.maxAttempts;
    cfg.reconnectDelaySec = reconn.delaySec;

    auto transport = std::make_unique<TcpClientTransport>(cfg);
    auto codec = std::make_unique<RawCodec>();
    return std::make_unique<StreamSession>(std::move(transport), std::move(codec));
}

std::string TcpProtocol::MakeDefaultFileName(
    int modeIndex, const std::map<std::string, FieldValue>& fields) const {
    auto sanitize = [](std::string s) {
        for (auto& c : s) {
            if (c == '.' || c == ':' || c == '/' || c == '\\') c = '_';
        }
        return s;
    };
    if (modeIndex == 1) {
        std::string h = sanitize(GetStr(fields, "listenHost", "0.0.0.0"));
        return "TCP_Server_" + h + "_" + std::to_string(GetInt(fields, "listenPort", 502)) +
               ".json";
    }
    std::string h = sanitize(GetStr(fields, "remoteHost", "127.0.0.1"));
    return "TCP_Client_" + h + "_" + std::to_string(GetInt(fields, "remotePort", 502)) + ".json";
}

ITestStepPtr TcpProtocol::CreateTestStep(const std::string& stepType,
                                         const nlohmann::json& params) {
    if (stepType == "send" || stepType == "tcp_send") {
        return std::make_unique<TcpSendStep>(params);
    }
    return nullptr;
}

}  // namespace icu
