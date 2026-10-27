#include <wx/string.h>

#include <fstream>
#include <map>
#include <string>

#include "config/ConnectionProfile.h"
#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "core/SessionConfig.h"
#include "doctest.h"
#include "helpers/TestEnv.h"

using namespace icu;
using icu::test::TestEnv;

namespace {

IProtocol* Tcp() {
    static bool registered = false;
    if (!registered) {
        RegisterBuiltinProtocols();
        registered = true;
    }
    return ProtocolRegistry::Instance().Find("tcp");
}

wxString Wx(const std::string& s) {
    return wxString::FromUTF8(s.c_str());
}

}  // namespace

TEST_CASE("ConnectionProfile: TCP Client roundtrip") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> fields;
    fields["role"] = FieldValue(std::string("Client"));
    fields["host"] = FieldValue(std::string("192.168.1.10"));
    fields["port"] = FieldValue(5020);
    fields["timeoutMs"] = FieldValue(5000);
    fields["maxClients"] = FieldValue(10);
    fields["autoReconnect"] = FieldValue(true);
    fields["reconnectMaxAttempts"] = FieldValue(std::string("3"));
    fields["reconnectDelaySec"] = FieldValue(std::string("5"));

    const wxString path = Wx(env.Path("TCP_Client_test.json"));

    REQUIRE(ConnectionProfile::Save(path, "tcp", 0, proto, fields, SessionConfig{}));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);
    CHECK(loaded.fields.size() == fields.size());

    CHECK(loaded.fields["role"].AsString() == "Client");
    CHECK(loaded.fields["host"].AsString() == "192.168.1.10");
    CHECK(loaded.fields["port"].AsInt() == 5020);
    CHECK(loaded.fields["timeoutMs"].AsInt() == 5000);
    CHECK(loaded.fields["maxClients"].AsInt() == 10);
    CHECK(loaded.fields["autoReconnect"].AsBool() == true);
    CHECK(loaded.fields["reconnectMaxAttempts"].AsString() == "3");
    CHECK(loaded.fields["reconnectDelaySec"].AsString() == "5");

    CHECK(loaded.fields["reconnectMaxAttempts"].GetType() == FieldValue::Type::Choice);
    CHECK(loaded.fields["reconnectDelaySec"].GetType() == FieldValue::Type::Choice);
    CHECK(loaded.fields["port"].GetType() == FieldValue::Type::Int);
    CHECK(loaded.fields["autoReconnect"].GetType() == FieldValue::Type::Bool);
}

TEST_CASE("ConnectionProfile: TCP Server roundtrip") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> fields;
    fields["role"] = FieldValue(std::string("Server"));
    fields["host"] = FieldValue(std::string("0.0.0.0"));
    fields["port"] = FieldValue(5030);
    fields["maxClients"] = FieldValue(20);

    const wxString path = Wx(env.Path("TCP_Server_test.json"));

    REQUIRE(ConnectionProfile::Save(path, "tcp", 0, proto, fields, SessionConfig{}));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);
    CHECK(loaded.fields["role"].AsString() == "Server");
    CHECK(loaded.fields["host"].AsString() == "0.0.0.0");
    CHECK(loaded.fields["port"].AsInt() == 5030);
    CHECK(loaded.fields["maxClients"].AsInt() == 20);
}

TEST_CASE("ConnectionProfile: UDP roundtrip") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> fields;
    fields["role"] = FieldValue(std::string("Client"));
    fields["host"] = FieldValue(std::string("127.0.0.1"));
    fields["port"] = FieldValue(5040);
    fields["localPort"] = FieldValue(0);

    const wxString path = Wx(env.Path("UDP_Client_test.json"));

    REQUIRE(ConnectionProfile::Save(path, "tcp", 1, proto, fields, SessionConfig{}));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 1);
    CHECK(loaded.fields["role"].AsString() == "Client");
    CHECK(loaded.fields["host"].AsString() == "127.0.0.1");
    CHECK(loaded.fields["port"].AsInt() == 5040);
    CHECK(loaded.fields["localPort"].AsInt() == 0);
}

TEST_CASE("ConnectionProfile: 空字段也能 Save/Load") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> empty;

    const wxString path = Wx(env.Path("empty.json"));
    REQUIRE(ConnectionProfile::Save(path, "tcp", 0, proto, empty, SessionConfig{}));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);
    CHECK(loaded.fields.empty());
}

TEST_CASE("ConnectionProfile: 不存在的文件 Load 失败") {
    TestEnv env;
    Tcp();

    const wxString path = Wx(env.Path("does_not_exist.json"));

    ActiveProfile loaded;
    CHECK_FALSE(ConnectionProfile::Load(path, loaded));
}

TEST_CASE("ConnectionProfile: 未知协议 Load 失败") {
    TestEnv env;
    Tcp();

    const std::string p = env.Path("unknown_proto.json");
    {
        std::ofstream ofs(p);
        ofs << R"({"connection":{"protocol":"nonexistent-protocol","mode":"Client"}})";
    }

    ActiveProfile loaded;
    CHECK_FALSE(ConnectionProfile::Load(wxString::FromUTF8(p.c_str()), loaded));
}

TEST_CASE("ConnectionProfile: Mode 名不匹配时回退到 modeIndex 0") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    const std::string p = env.Path("bad_mode.json");
    {
        std::ofstream ofs(p);
        ofs << R"({"connection":{"protocol":"tcp","mode":"UnknownMode",)"
               R"("host":"127.0.0.1","port":502}})";
    }

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(wxString::FromUTF8(p.c_str()), loaded));
    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);

    CHECK(loaded.fields["host"].AsString() == "127.0.0.1");
    CHECK(loaded.fields["port"].AsInt() == 502);
}

TEST_CASE("ConnectionProfile: MakeDefaultFileName") {
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    SUBCASE("Client") {
        std::map<std::string, FieldValue> fields;
        fields["role"] = FieldValue(std::string("Client"));
        fields["host"] = FieldValue(std::string("127.0.0.1"));
        fields["port"] = FieldValue(502);

        const wxString name = ConnectionProfile::MakeDefaultFileName(proto, 0, fields);
        CHECK(std::string(name.utf8_str()) == "TCP_Client_127_0_0_1_502.json");
    }

    SUBCASE("Server") {
        std::map<std::string, FieldValue> fields;
        fields["role"] = FieldValue(std::string("Server"));
        fields["host"] = FieldValue(std::string("0.0.0.0"));
        fields["port"] = FieldValue(5030);

        const wxString name = ConnectionProfile::MakeDefaultFileName(proto, 0, fields);
        CHECK(std::string(name.utf8_str()) == "TCP_Server_0_0_0_0_5030.json");
    }

    SUBCASE("UDP") {
        std::map<std::string, FieldValue> fields;
        fields["role"] = FieldValue(std::string("Client"));
        fields["host"] = FieldValue(std::string("127.0.0.1"));
        fields["port"] = FieldValue(5040);

        const wxString name = ConnectionProfile::MakeDefaultFileName(proto, 1, fields);
        CHECK(std::string(name.utf8_str()) == "UDP_Client_127_0_0_1_5040.json");
    }

    SUBCASE("未知协议指针 → Untitled") {
        const wxString name = ConnectionProfile::MakeDefaultFileName(nullptr, 0, {});
        CHECK(std::string(name.utf8_str()) == "Untitled.json");
    }
}
