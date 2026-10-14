#include "core/ProtocolTypes.h"
#include "doctest.h"

using namespace icu;

TEST_CASE("FieldValue: String 序列化往返") {
    FieldValue v(std::string("127.0.0.1"));
    CHECK(v.GetType() == FieldValue::Type::String);
    CHECK(v.AsString() == "127.0.0.1");
    CHECK(v.Serialize() == "127.0.0.1");

    auto v2 = FieldValue::Deserialize("127.0.0.1", FieldValue::Type::String);
    CHECK(v == v2);
}

TEST_CASE("FieldValue: Int 序列化往返") {
    FieldValue v(502);
    CHECK(v.GetType() == FieldValue::Type::Int);
    CHECK(v.AsInt() == 502);
    CHECK(v.Serialize() == "502");

    auto v2 = FieldValue::Deserialize("502", FieldValue::Type::Int);
    CHECK(v == v2);
}

TEST_CASE("FieldValue: Int 反序列化非法输入") {
    auto v = FieldValue::Deserialize("abc", FieldValue::Type::Int);
    CHECK(v.AsInt() == 0);
}

TEST_CASE("FieldValue: Bool 序列化往返") {
    FieldValue t(true);
    FieldValue f(false);

    CHECK(t.Serialize() == "1");
    CHECK(f.Serialize() == "0");

    CHECK(FieldValue::Deserialize("1", FieldValue::Type::Bool).AsBool());
    CHECK(FieldValue::Deserialize("true", FieldValue::Type::Bool).AsBool());
    CHECK(FieldValue::Deserialize("yes", FieldValue::Type::Bool).AsBool());
    CHECK_FALSE(FieldValue::Deserialize("0", FieldValue::Type::Bool).AsBool());
    CHECK_FALSE(FieldValue::Deserialize("", FieldValue::Type::Bool).AsBool());
}

TEST_CASE("FieldValue: Choice 序列化往返") {
    FieldValue v(std::string("3"));
    auto c = FieldValue::Deserialize("3", FieldValue::Type::Choice);
    CHECK(c.GetType() == FieldValue::Type::Choice);
    CHECK(c.AsChoice() == "3");
    CHECK(c.Serialize() == "3");
}

TEST_CASE("FieldValue: operator== 类型不同则不相等") {
    FieldValue s(std::string("502"));
    FieldValue i(502);
    CHECK(s != i);
}

TEST_CASE("FieldValue: operator== 同类型比较") {
    FieldValue a(502);
    FieldValue b(502);
    FieldValue c(503);

    CHECK(a == b);
    CHECK(a != c);
}
