#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "doctest.h"

using namespace icu;

TEST_CASE("ProtocolRegistry: 单例唯一") {
    auto& a = ProtocolRegistry::Instance();
    auto& b = ProtocolRegistry::Instance();
    CHECK(&a == &b);
}

TEST_CASE("ProtocolRegistry: 注册内置协议后能查到 tcp") {
    RegisterBuiltinProtocols();

    auto* proto = ProtocolRegistry::Instance().Find("tcp");
    REQUIRE(proto != nullptr);
    CHECK(proto->Id() == "tcp");
}

TEST_CASE("ProtocolRegistry: 未知 ID 返回 nullptr") {
    auto* proto = ProtocolRegistry::Instance().Find("nonexistent-protocol");
    CHECK(proto == nullptr);
}

TEST_CASE("ProtocolRegistry: All 包含 tcp") {
    RegisterBuiltinProtocols();

    auto all = ProtocolRegistry::Instance().All();
    CHECK_FALSE(all.empty());

    bool found = false;
    for (auto* p : all) {
        if (p->Id() == "tcp") {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("TcpIpProtocol: 基本属性") {
    RegisterBuiltinProtocols();
    auto* proto = ProtocolRegistry::Instance().Find("tcp");
    REQUIRE(proto != nullptr);

    CHECK(proto->Id() == "tcp");
    CHECK(proto->DisplayName().find("TCP/IP") != std::string::npos);
    CHECK(proto->HasModes() == true);

    auto modes = proto->ModeNames();
    REQUIRE(modes.size() == 2);
    CHECK(modes[0] == "TCP");
    CHECK(modes[1] == "UDP");
}

TEST_CASE("TcpIpProtocol: FieldsForMode 数量") {
    RegisterBuiltinProtocols();
    auto* proto = ProtocolRegistry::Instance().Find("tcp");
    REQUIRE(proto != nullptr);

    auto tcpFields = proto->FieldsForMode(0);
    auto udpFields = proto->FieldsForMode(1);

    // TCP = role/host/port/timeoutMs/maxClients + 3 个重连字段
    CHECK(tcpFields.size() >= 8);
    // UDP = role/host/port/localPort
    CHECK(udpFields.size() >= 4);

    auto hasRole = [](const std::vector<FieldDescriptor>& fields) {
        for (const auto& fd : fields)
            if (fd.key == "role") return true;
        return false;
    };
    CHECK(hasRole(tcpFields));
    CHECK(hasRole(udpFields));
}

TEST_CASE("TcpProtocol: label 是翻译 key（非空）") {
    RegisterBuiltinProtocols();
    auto* proto = ProtocolRegistry::Instance().Find("tcp");
    REQUIRE(proto != nullptr);

    for (auto& fd : proto->FieldsForMode(0)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
    for (auto& fd : proto->FieldsForMode(1)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
}
