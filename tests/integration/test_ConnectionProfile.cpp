#include "config/ConnectionProfile.h"
#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "doctest.h"
#include "helpers/TestEnv.h"

#include <wx/fileconf.h>
#include <wx/string.h>

#include <map>
#include <string>

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
    fields["remoteHost"]           = FieldValue(std::string("192.168.1.10"));
    fields["remotePort"]           = FieldValue(5020);
    fields["localHost"]            = FieldValue(std::string("0.0.0.0"));
    fields["localPort"]            = FieldValue(0);
    fields["timeoutMs"]            = FieldValue(5000);
    fields["autoReconnect"]        = FieldValue(true);
    fields["reconnectMaxAttempts"] = FieldValue(std::string("3"));
    fields["reconnectDelaySec"]    = FieldValue(std::string("5"));

    const wxString path = Wx(env.Path("TCP_Client_test.conn"));

    REQUIRE(ConnectionProfile::Save(path, "tcp", 0, proto, fields));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);
    CHECK(loaded.fields.size() == fields.size());

    CHECK(loaded.fields["remoteHost"].AsString() == "192.168.1.10");
    CHECK(loaded.fields["remotePort"].AsInt()    == 5020);
    CHECK(loaded.fields["localHost"].AsString()  == "0.0.0.0");
    CHECK(loaded.fields["localPort"].AsInt()     == 0);
    CHECK(loaded.fields["timeoutMs"].AsInt()     == 5000);
    CHECK(loaded.fields["autoReconnect"].AsBool() == true);
    CHECK(loaded.fields["reconnectMaxAttempts"].AsString() == "3");
    CHECK(loaded.fields["reconnectDelaySec"].AsString()    == "5");

    CHECK(loaded.fields["reconnectMaxAttempts"].GetType() == FieldValue::Type::Choice);
    CHECK(loaded.fields["reconnectDelaySec"].GetType()    == FieldValue::Type::Choice);
    CHECK(loaded.fields["remotePort"].GetType()           == FieldValue::Type::Int);
    CHECK(loaded.fields["autoReconnect"].GetType()        == FieldValue::Type::Bool);
}

TEST_CASE("ConnectionProfile: TCP Server roundtrip") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> fields;
    fields["listenHost"] = FieldValue(std::string("0.0.0.0"));
    fields["listenPort"] = FieldValue(5030);
    fields["maxClients"] = FieldValue(20);
    fields["backlog"]    = FieldValue(10);

    const wxString path = Wx(env.Path("TCP_Server_test.conn"));

    REQUIRE(ConnectionProfile::Save(path, "tcp", 1, proto, fields));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 1);
    CHECK(loaded.fields["listenHost"].AsString() == "0.0.0.0");
    CHECK(loaded.fields["listenPort"].AsInt()    == 5030);
    CHECK(loaded.fields["maxClients"].AsInt()    == 20);
    CHECK(loaded.fields["backlog"].AsInt()       == 10);
}

TEST_CASE("ConnectionProfile: 空字段也能 Save/Load") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    std::map<std::string, FieldValue> empty;

    const wxString path = Wx(env.Path("empty.conn"));
    REQUIRE(ConnectionProfile::Save(path, "tcp", 0, proto, empty));

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));

    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);
    CHECK(loaded.fields.empty());
}

TEST_CASE("ConnectionProfile: 不存在的文件 Load 失败") {
    TestEnv env;
    Tcp();

    const wxString path = Wx(env.Path("does_not_exist.conn"));

    ActiveProfile loaded;
    CHECK_FALSE(ConnectionProfile::Load(path, loaded));
}

TEST_CASE("ConnectionProfile: 未知协议 Load 失败") {
    TestEnv env;
    Tcp();

    const wxString path = Wx(env.Path("unknown_proto.conn"));

    {
        wxFileConfig fc(wxEmptyString, wxEmptyString, path,
                        wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
        fc.Write("/Connection/Protocol", "nonexistent-protocol");
        fc.Write("/Connection/Mode",     "Client");
        fc.Flush();
    }

    ActiveProfile loaded;
    CHECK_FALSE(ConnectionProfile::Load(path, loaded));
}

TEST_CASE("ConnectionProfile: Mode 名不匹配时回退到 modeIndex 0") {
    TestEnv env;
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    const wxString path = Wx(env.Path("bad_mode.conn"));

    {
        wxFileConfig fc(wxEmptyString, wxEmptyString, path,
                        wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
        fc.Write("/Connection/Protocol",   "tcp");
        fc.Write("/Connection/Mode",       "UnknownMode");
        fc.Write("/Connection/remoteHost", "127.0.0.1");
        fc.Write("/Connection/remotePort", "502");
        fc.Flush();
    }

    ActiveProfile loaded;
    REQUIRE(ConnectionProfile::Load(path, loaded));
    CHECK(loaded.protocolId == "tcp");
    CHECK(loaded.modeIndex == 0);

    CHECK(loaded.fields["remoteHost"].AsString() == "127.0.0.1");
    CHECK(loaded.fields["remotePort"].AsInt()    == 502);
}

TEST_CASE("ConnectionProfile: MakeDefaultFileName") {
    auto* proto = Tcp();
    REQUIRE(proto != nullptr);

    SUBCASE("Client") {
        std::map<std::string, FieldValue> fields;
        fields["remoteHost"] = FieldValue(std::string("127.0.0.1"));
        fields["remotePort"] = FieldValue(502);

        const wxString name = ConnectionProfile::MakeDefaultFileName(proto, 0, fields);
        CHECK(std::string(name.utf8_str()) == "TCP_Client_127_0_0_1_502.conn");
    }

    SUBCASE("Server") {
        std::map<std::string, FieldValue> fields;
        fields["listenHost"] = FieldValue(std::string("0.0.0.0"));
        fields["listenPort"] = FieldValue(5030);

        const wxString name = ConnectionProfile::MakeDefaultFileName(proto, 1, fields);
        CHECK(std::string(name.utf8_str()) == "TCP_Server_0_0_0_0_5030.conn");
    }

    SUBCASE("未知协议指针 → Untitled") {
        const wxString name = ConnectionProfile::MakeDefaultFileName(
            nullptr, 0, {});
        CHECK(std::string(name.utf8_str()) == "Untitled.conn");
    }
}
