#include <map>
#include <set>
#include <string>

#include "core/ProtocolRegistry.h"
#include "core/StreamSession.h"
#include "doctest.h"
#include "protocols/tcpip/TcpIpProtocol.h"

using namespace icu;

TEST_CASE("TcpIpProtocol: 基本属性") {
    TcpIpProtocol p;
    CHECK(p.Id() == "tcp");
    CHECK(p.DisplayName() == "&TCP/IP");
    CHECK(p.HasModes() == true);

    auto modes = p.ModeNames();
    REQUIRE(modes.size() == 2);
    CHECK(modes[0] == "TCP");
    CHECK(modes[1] == "UDP");
}

TEST_CASE("TcpIpProtocol: FieldsForMode 非空") {
    TcpIpProtocol p;
    auto tcpFields = p.FieldsForMode(0);
    auto udpFields = p.FieldsForMode(1);

    CHECK(tcpFields.size() >= 4);
    CHECK(udpFields.size() >= 4);
}

TEST_CASE("TcpIpProtocol: 关键字段存在") {
    TcpIpProtocol p;

    SUBCASE("TCP") {
        auto fields = p.FieldsForMode(0);
        std::set<std::string> keys;
        for (const auto& fd : fields) keys.insert(fd.key);

        CHECK(keys.count("role") == 1);
        CHECK(keys.count("host") == 1);
        CHECK(keys.count("port") == 1);
    }

    SUBCASE("UDP") {
        auto fields = p.FieldsForMode(1);
        std::set<std::string> keys;
        for (const auto& fd : fields) keys.insert(fd.key);

        CHECK(keys.count("role") == 1);
        CHECK(keys.count("host") == 1);
        CHECK(keys.count("port") == 1);
        CHECK(keys.count("localPort") == 1);
    }
}

TEST_CASE("TcpIpProtocol: label 是翻译 key（非空）") {
    TcpIpProtocol p;
    for (const auto& fd : p.FieldsForMode(0)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
    for (const auto& fd : p.FieldsForMode(1)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
}

TEST_CASE("TcpIpProtocol: MakeDefaultFileName") {
    TcpIpProtocol p;

    SUBCASE("TCP Client") {
        std::map<std::string, FieldValue> fields;
        fields["role"] = FieldValue(std::string("Client"));
        fields["host"] = FieldValue(std::string("192.168.1.10"));
        fields["port"] = FieldValue(502);

        const std::string name = p.MakeDefaultFileName(0, fields);
        CHECK(name == "TCP_Client_192_168_1_10_502.json");
    }

    SUBCASE("UDP Server") {
        std::map<std::string, FieldValue> fields;
        fields["role"] = FieldValue(std::string("Server"));
        fields["host"] = FieldValue(std::string("0.0.0.0"));
        fields["port"] = FieldValue(5020);

        const std::string name = p.MakeDefaultFileName(1, fields);
        CHECK(name == "UDP_Server_0_0_0_0_5020.json");
    }
}

TEST_CASE("TcpIpProtocol: CreateSession 返回可用会话") {
    TcpIpProtocol p;
    std::map<std::string, FieldValue> fields;
    fields["role"] = FieldValue(std::string("Client"));
    fields["host"] = FieldValue(std::string("127.0.0.1"));
    fields["port"] = FieldValue(502);

    auto session = p.CreateSession(0, fields);
    REQUIRE(session != nullptr);
    CHECK(session->State() == SessionState::Idle);
    CHECK_FALSE(session->IsConnected());
}

TEST_CASE("TcpIpProtocol: 注册到 ProtocolRegistry") {
    RegisterBuiltinProtocols();
    auto* proto = ProtocolRegistry::Instance().Find("tcp");
    REQUIRE(proto != nullptr);
    CHECK(proto->Id() == "tcp");
}
