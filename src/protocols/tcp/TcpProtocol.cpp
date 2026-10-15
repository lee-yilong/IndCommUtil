#include "TcpProtocol.h"

#include "TcpTransport.h"
#include "core/Session.h"
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
    return {
        {"remoteHost", wxTRANSLATE("Remote Host:"), FieldValue::Type::String,
         FieldValue(std::string("127.0.0.1"))},

        {"remotePort", wxTRANSLATE("Remote Port:"), FieldValue::Type::Int, FieldValue(502), 1,
         65535},

        {"localHost", wxTRANSLATE("Local Host (optional):"), FieldValue::Type::String,
         FieldValue(std::string("0.0.0.0"))},

        {"localPort", wxTRANSLATE("Local Port (0 = auto):"), FieldValue::Type::Int, FieldValue(0),
         0, 65535},

        {"timeoutMs", wxTRANSLATE("Connect Timeout (ms):"), FieldValue::Type::Int, FieldValue(3000),
         100, 600000},

        {"autoReconnect", wxTRANSLATE("Auto reconnect on disconnect"), FieldValue::Type::Bool,
         FieldValue(true)},

        {"reconnectMaxAttempts",
         wxTRANSLATE("Max reconnect attempts:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("0")),
         0,
         0,
         {{"0", wxTRANSLATE("Unlimited")},
          {"1", wxTRANSLATE("1")},
          {"3", wxTRANSLATE("3")},
          {"5", wxTRANSLATE("5")},
          {"10", wxTRANSLATE("10")},
          {"30", wxTRANSLATE("30")}}},

        {"reconnectDelaySec",
         wxTRANSLATE("Reconnect delay:"),
         FieldValue::Type::Choice,
         FieldValue(std::string("0")),
         0,
         0,
         {{"0", wxTRANSLATE("Auto")},
          {"1", wxTRANSLATE("1 s")},
          {"3", wxTRANSLATE("3 s")},
          {"5", wxTRANSLATE("5 s")},
          {"10", wxTRANSLATE("10 s")},
          {"30", wxTRANSLATE("30 s")}}},
    };
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

SessionPtr TcpProtocol::CreateSession(int modeIndex,
                                      const std::map<std::string, FieldValue>& fields) {
    if (modeIndex == 1) {
        // ---------- Server ----------
        TcpServerConfig cfg;
        cfg.listenHost = GetStr(fields, "listenHost", "0.0.0.0");
        cfg.listenPort = GetInt(fields, "listenPort", 502);
        cfg.maxClients = GetInt(fields, "maxClients", 10);
        cfg.backlog = GetInt(fields, "backlog", 5);

        auto transport = std::make_unique<TcpServerTransport>(cfg);
        auto codec = std::make_unique<RawCodec>();
        return std::make_unique<Session>(std::move(transport), std::move(codec));
    }

    // ---------- Client ----------
    TcpClientConfig cfg;
    cfg.remoteHost = GetStr(fields, "remoteHost", "127.0.0.1");
    cfg.remotePort = GetInt(fields, "remotePort", 502);
    cfg.connectTimeoutMs = GetInt(fields, "timeoutMs", 3000);
    cfg.autoReconnect = GetBool(fields, "autoReconnect", true);

    {
        auto it = fields.find("reconnectMaxAttempts");
        cfg.reconnectMaxAttempts = (it == fields.end()) ? 0 : std::stoi(it->second.AsString());
    }
    {
        auto it = fields.find("reconnectDelaySec");
        cfg.reconnectDelaySec = (it == fields.end()) ? 0 : std::stoi(it->second.AsString());
    }

    auto transport = std::make_unique<TcpClientTransport>(cfg);
    auto codec = std::make_unique<RawCodec>();
    return std::make_unique<Session>(std::move(transport), std::move(codec));
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
               ".conn";
    }

    std::string h = sanitize(GetStr(fields, "remoteHost", "127.0.0.1"));
    return "TCP_Client_" + h + "_" + std::to_string(GetInt(fields, "remotePort", 502)) + ".conn";
}

}  // namespace icu
