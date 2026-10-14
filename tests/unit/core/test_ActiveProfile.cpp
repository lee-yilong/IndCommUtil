#include "core/ProtocolTypes.h"
#include "doctest.h"

using namespace icu;

TEST_CASE("ActiveProfile: 默认构造为空") {
    ActiveProfile p;
    CHECK(p.Empty());
    CHECK(p.protocolId.empty());
    CHECK(p.modeIndex == 0);
    CHECK(p.fields.empty());
}

TEST_CASE("ActiveProfile: Clear 清空所有字段") {
    ActiveProfile p;
    p.protocolId = "tcp";
    p.modeIndex = 1;
    p.fields["host"] = FieldValue(std::string("127.0.0.1"));

    CHECK_FALSE(p.Empty());

    p.Clear();
    CHECK(p.Empty());
    CHECK(p.modeIndex == 0);
    CHECK(p.fields.empty());
}

TEST_CASE("ActiveProfile: operator== 完全一致") {
    ActiveProfile a;
    a.protocolId = "tcp";
    a.modeIndex = 0;
    a.fields["host"] = FieldValue(std::string("127.0.0.1"));
    a.fields["port"] = FieldValue(502);

    ActiveProfile b = a;
    CHECK(a == b);
}

TEST_CASE("ActiveProfile: protocolId 不同则不等") {
    ActiveProfile a, b;
    a.protocolId = "tcp";
    b.protocolId = "serial";
    CHECK(a != b);
}

TEST_CASE("ActiveProfile: modeIndex 不同则不等") {
    ActiveProfile a, b;
    a.protocolId = "tcp";
    a.modeIndex = 0;
    b.protocolId = "tcp";
    b.modeIndex = 1;
    CHECK(a != b);
}

TEST_CASE("ActiveProfile: fields 值不同则不等") {
    ActiveProfile a, b;
    a.protocolId = "tcp";
    a.fields["port"] = FieldValue(502);

    b.protocolId = "tcp";
    b.fields["port"] = FieldValue(503);

    CHECK(a != b);
}

TEST_CASE("ActiveProfile: fields 数量不同则不等") {
    ActiveProfile a, b;
    a.protocolId = "tcp";
    a.fields["host"] = FieldValue(std::string("127.0.0.1"));
    a.fields["port"] = FieldValue(502);

    b.protocolId = "tcp";
    b.fields["host"] = FieldValue(std::string("127.0.0.1"));

    CHECK(a != b);
}
