#include <map>
#include <set>
#include <string>

#include "core/ProtocolRegistry.h"
#include "core/Session.h"
#include "doctest.h"
#include "protocols/serial/SerialProtocol.h"

using namespace icu;

namespace {

const FieldDescriptor* FindField(const std::vector<FieldDescriptor>& fields,
                                 const std::string& key) {
    for (const auto& fd : fields) {
        if (fd.key == key) return &fd;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("SerialProtocol: 基本属性") {
    SerialProtocol p;
    CHECK(p.Id() == "serial");
    CHECK_FALSE(p.DisplayName().empty());
    CHECK(p.HasModes() == false);
    CHECK(p.ModeNames().empty());
}

TEST_CASE("SerialProtocol: FieldsForMode 非空") {
    SerialProtocol p;
    auto fields = p.FieldsForMode(0);
    CHECK(fields.size() >= 5);
}

TEST_CASE("SerialProtocol: 关键字段存在") {
    SerialProtocol p;
    auto fields = p.FieldsForMode(0);

    std::set<std::string> keys;
    for (const auto& fd : fields) keys.insert(fd.key);

    CHECK(keys.count("portName") == 1);
    CHECK(keys.count("baudRate") == 1);
    CHECK(keys.count("dataBits") == 1);
    CHECK(keys.count("parity") == 1);
    CHECK(keys.count("stopBits") == 1);
    CHECK(keys.count("flowControl") == 1);
    CHECK(keys.count("readTimeoutMs") == 1);
    CHECK(keys.count("writeTimeoutMs") == 1);
}

TEST_CASE("SerialProtocol: label 是翻译 key（非空）") {
    SerialProtocol p;
    for (const auto& fd : p.FieldsForMode(0)) {
        CHECK_FALSE(fd.key.empty());
        CHECK_FALSE(fd.label.empty());
    }
}

TEST_CASE("SerialProtocol: 字段类型正确") {
    SerialProtocol p;
    auto fields = p.FieldsForMode(0);

    const auto* portName = FindField(fields, "portName");
    REQUIRE(portName != nullptr);
    CHECK(portName->type == FieldValue::Type::Choice);
    CHECK_FALSE(portName->choices.empty());
    CHECK_FALSE(portName->choices.empty());

    const auto* baud = FindField(fields, "baudRate");
    REQUIRE(baud != nullptr);
    CHECK(baud->type == FieldValue::Type::Choice);
    CHECK_FALSE(baud->choices.empty());

    const auto* parity = FindField(fields, "parity");
    REQUIRE(parity != nullptr);
    CHECK(parity->type == FieldValue::Type::Choice);

    const auto* timeout = FindField(fields, "readTimeoutMs");
    REQUIRE(timeout != nullptr);
    CHECK(timeout->type == FieldValue::Type::Int);
}

TEST_CASE("SerialProtocol: MakeDefaultFileName") {
    SerialProtocol p;

    std::map<std::string, FieldValue> fields;
    fields["portName"] = FieldValue(std::string("COM3"));
    fields["baudRate"] = FieldValue(std::string("9600"));

    const std::string name = p.MakeDefaultFileName(0, fields);
    CHECK(name == "Serial_COM3_9600.json");
}

TEST_CASE("SerialProtocol: MakeDefaultFileName 转义特殊字符") {
    SerialProtocol p;

    std::map<std::string, FieldValue> fields;
    fields["portName"] = FieldValue(std::string("/dev/ttyUSB0"));
    fields["baudRate"] = FieldValue(std::string("115200"));

    const std::string name = p.MakeDefaultFileName(0, fields);
    CHECK(name == "Serial__dev_ttyUSB0_115200.json");
}

TEST_CASE("SerialProtocol: CreateSession 返回 Idle 状态的 Session") {
    SerialProtocol p;

    std::map<std::string, FieldValue> fields;
    fields["portName"] = FieldValue(std::string("COM99"));
    fields["baudRate"] = FieldValue(std::string("9600"));
    fields["dataBits"] = FieldValue(std::string("8"));
    fields["parity"] = FieldValue(std::string("N"));
    fields["stopBits"] = FieldValue(std::string("1"));
    fields["flowControl"] = FieldValue(std::string("None"));

    auto session = p.CreateSession(0, fields);
    REQUIRE(session != nullptr);
    CHECK(session->State() == SessionState::Idle);
    CHECK_FALSE(session->IsConnected());
}

TEST_CASE("SerialProtocol: 注册到 ProtocolRegistry") {
    RegisterBuiltinProtocols();

    auto* proto = ProtocolRegistry::Instance().Find("serial");
    REQUIRE(proto != nullptr);
    CHECK(proto->Id() == "serial");
}

TEST_CASE("ProtocolRegistry: TCP 注册在 Serial 之前") {
    RegisterBuiltinProtocols();

    auto all = ProtocolRegistry::Instance().All();
    REQUIRE(all.size() >= 2);

    CHECK(all[0]->Id() == "tcp");
    CHECK(all[1]->Id() == "serial");
}
