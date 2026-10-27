#include "core/ValueValidator.h"
#include "doctest.h"

using namespace icu;

TEST_CASE("ValueValidator: 整数类型与范围") {
    std::string err;

    CHECK(ValueValidator::Validate("int32", "123", err));
    CHECK(ValueValidator::Validate("int32", "-2147483648", err));
    CHECK(ValueValidator::Validate("int32", "2147483647", err));
    CHECK_FALSE(ValueValidator::Validate("int32", "2147483648", err));
    CHECK_FALSE(ValueValidator::Validate("int32", "12.5", err));
    CHECK_FALSE(ValueValidator::Validate("int32", "abc", err));
    CHECK_FALSE(ValueValidator::Validate("int32", "", err));
    CHECK_FALSE(ValueValidator::Validate("int32", "1 2", err));

    CHECK(ValueValidator::Validate("int8", "-128", err));
    CHECK_FALSE(ValueValidator::Validate("int8", "-129", err));
    CHECK(ValueValidator::Validate("uint8", "255", err));
    CHECK_FALSE(ValueValidator::Validate("uint8", "256", err));
    CHECK_FALSE(ValueValidator::Validate("uint16", "-1", err));

    CHECK(ValueValidator::Validate("uint32", "4294967295", err));
    CHECK_FALSE(ValueValidator::Validate("uint32", "4294967296", err));
}

TEST_CASE("ValueValidator: 浮点与布尔") {
    std::string err;

    CHECK(ValueValidator::Validate("float", "3.14", err));
    CHECK(ValueValidator::Validate("double", "-1e9", err));
    CHECK_FALSE(ValueValidator::Validate("double", "nan", err));
    CHECK_FALSE(ValueValidator::Validate("double", "1.2x", err));

    CHECK(ValueValidator::Validate("bool", "true", err));
    CHECK(ValueValidator::Validate("bool", "FALSE", err));
    CHECK(ValueValidator::Validate("bool", "1", err));
    CHECK(ValueValidator::Validate("bool", "0", err));
    CHECK_FALSE(ValueValidator::Validate("bool", "maybe", err));
}

TEST_CASE("ValueValidator: Hex 编码") {
    std::string err;
    std::vector<uint8_t> bytes;

    CHECK(ValueValidator::Validate("hex", "01 0A ff", err));
    CHECK(ValueValidator::Encode("hex", "01 0A ff", bytes, err));
    REQUIRE(bytes.size() == 3);
    CHECK(bytes[0] == 0x01);
    CHECK(bytes[1] == 0x0A);
    CHECK(bytes[2] == 0xFF);

    CHECK(ValueValidator::Encode("hex", "0x01,0x02", bytes, err));
    REQUIRE(bytes.size() == 2);
    CHECK(bytes[0] == 0x01);
    CHECK(bytes[1] == 0x02);

    CHECK_FALSE(ValueValidator::Validate("hex", "0A0", err));
    CHECK_FALSE(ValueValidator::Validate("hex", "GG", err));
    CHECK_FALSE(ValueValidator::Validate("hex", "", err));
}

TEST_CASE("ValueValidator: 编码结果") {
    std::string err;
    std::vector<uint8_t> bytes;

    // 整数字段不接受 0x 前缀
    CHECK_FALSE(ValueValidator::Encode("int16", "0x0102", bytes, err));

    REQUIRE(ValueValidator::Encode("int16", "258", bytes, err));
    REQUIRE(bytes.size() == 2);
    CHECK(bytes[0] == 0x01);
    CHECK(bytes[1] == 0x02);

    REQUIRE(ValueValidator::Encode("uint32", "16909060", bytes, err));
    REQUIRE(bytes.size() == 4);
    CHECK(bytes[0] == 0x01);
    CHECK(bytes[1] == 0x02);
    CHECK(bytes[2] == 0x03);
    CHECK(bytes[3] == 0x04);

    REQUIRE(ValueValidator::Encode("int32", "-1", bytes, err));
    REQUIRE(bytes.size() == 4);
    CHECK(bytes[0] == 0xFF);
    CHECK(bytes[1] == 0xFF);
    CHECK(bytes[2] == 0xFF);
    CHECK(bytes[3] == 0xFF);

    REQUIRE(ValueValidator::Encode("string", "hello", bytes, err));
    REQUIRE(bytes.size() == 5);
    CHECK(bytes[0] == 'h');

    REQUIRE(ValueValidator::Encode("bool", "true", bytes, err));
    REQUIRE(bytes.size() == 1);
    CHECK(bytes[0] == 1);
}
