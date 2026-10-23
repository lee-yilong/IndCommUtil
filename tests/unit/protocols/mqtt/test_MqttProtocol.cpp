#include <map>
#include <set>
#include <string>

#include "core/ProtocolRegistry.h"
#include "core/StreamSession.h"
#include "doctest.h"
#include "protocols/mqtt/MqttBrokerSession.h"
#include "protocols/mqtt/MqttProtocol.h"
#include "protocols/mqtt/MqttSession.h"

using namespace icu;

TEST_CASE("MqttProtocol: 基本属性") {
    MqttProtocol p;
    CHECK(p.Id() == "mqtt");
    CHECK(p.DisplayName() == "&MQTT");
    CHECK(p.HasModes() == true);

    auto modes = p.ModeNames();
    REQUIRE(modes.size() == 3);
    CHECK(modes[0] == "Publisher");
    CHECK(modes[1] == "Subscriber");
    CHECK(modes[2] == "Broker");
}

TEST_CASE("MqttProtocol: FieldsForMode 非空") {
    MqttProtocol p;
    CHECK(p.FieldsForMode(0).size() >= 4);
    CHECK(p.FieldsForMode(1).size() >= 6);
    CHECK(p.FieldsForMode(2).size() >= 1);
}

TEST_CASE("MqttProtocol: 关键字段存在") {
    MqttProtocol p;

    SUBCASE("Publisher") {
        auto fields = p.FieldsForMode(0);
        std::set<std::string> keys;
        for (const auto& fd : fields) keys.insert(fd.key);
        CHECK(keys.count("brokerHost") == 1);
        CHECK(keys.count("brokerPort") == 1);
        CHECK(keys.count("clientId") == 1);
    }

    SUBCASE("Subscriber") {
        auto fields = p.FieldsForMode(1);
        std::set<std::string> keys;
        for (const auto& fd : fields) keys.insert(fd.key);
        CHECK(keys.count("brokerHost") == 1);
        CHECK(keys.count("brokerPort") == 1);
        CHECK(keys.count("topic") == 1);
        CHECK(keys.count("qos") == 1);
    }

    SUBCASE("Broker") {
        auto fields = p.FieldsForMode(2);
        std::set<std::string> keys;
        for (const auto& fd : fields) keys.insert(fd.key);
        CHECK(keys.count("listenPort") == 1);
    }
}

TEST_CASE("MqttProtocol: label 是翻译 key（非空）") {
    MqttProtocol p;
    for (const auto& fd : p.FieldsForMode(0)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
    for (const auto& fd : p.FieldsForMode(1)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
    for (const auto& fd : p.FieldsForMode(2)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
}

TEST_CASE("MqttProtocol: MakeDefaultFileName") {
    MqttProtocol p;

    SUBCASE("Publisher") {
        std::map<std::string, FieldValue> fields;
        fields["brokerHost"] = FieldValue(std::string("192.168.1.10"));
        fields["brokerPort"] = FieldValue(1883);

        const std::string name = p.MakeDefaultFileName(0, fields);
        CHECK(name == "MQTT_Publisher_192_168_1_10_1883.json");
    }

    SUBCASE("Broker") {
        std::map<std::string, FieldValue> fields;
        fields["listenPort"] = FieldValue(1884);

        const std::string name = p.MakeDefaultFileName(2, fields);
        CHECK(name == "MQTT_Broker_1884.json");
    }
}

TEST_CASE("MqttProtocol: CreateSession 返回可用会话") {
    MqttProtocol p;
    std::map<std::string, FieldValue> fields;
    fields["brokerHost"] = FieldValue(std::string("127.0.0.1"));
    fields["brokerPort"] = FieldValue(1883);

    SUBCASE("Publisher") {
        auto session = p.CreateSession(0, fields);
        REQUIRE(session != nullptr);
        CHECK(session->State() == SessionState::Idle);
        CHECK(dynamic_cast<MqttSession*>(session.get()) != nullptr);
    }

    SUBCASE("Subscriber") {
        auto session = p.CreateSession(1, fields);
        REQUIRE(session != nullptr);
        CHECK(session->State() == SessionState::Idle);
        CHECK(dynamic_cast<MqttSession*>(session.get()) != nullptr);
    }

    SUBCASE("Broker") {
        std::map<std::string, FieldValue> bFields;
        bFields["listenPort"] = FieldValue(1883);
        auto session = p.CreateSession(2, bFields);
        REQUIRE(session != nullptr);
        CHECK(session->State() == SessionState::Idle);
        CHECK(dynamic_cast<MqttBrokerSession*>(session.get()) != nullptr);
    }
}

TEST_CASE("MqttProtocol: 注册到 ProtocolRegistry") {
    RegisterBuiltinProtocols();
    auto* proto = ProtocolRegistry::Instance().Find("mqtt");
    REQUIRE(proto != nullptr);
    CHECK(proto->Id() == "mqtt");
}
