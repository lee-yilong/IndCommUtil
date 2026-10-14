#include "doctest.h"
#include "protocols/codec/RawCodec.h"

#include <string>
#include <vector>

using namespace icu;

namespace {

std::vector<uint8_t> Bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

std::string ToStr(const std::vector<uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

}  // namespace

TEST_CASE("RawCodec: Name 非空") {
    RawCodec c;
    CHECK_FALSE(c.Name().empty());
}

TEST_CASE("RawCodec: Encode 直通字节") {
    RawCodec c;

    const std::vector<uint8_t> msg = {'h', 'e', 'l', 'l', 'o'};
    std::vector<uint8_t> out;

    CHECK(c.Encode(msg, out));
    CHECK(out == msg);
}

TEST_CASE("RawCodec: Encode 空消息 → 空输出，仍返回 true") {
    RawCodec c;

    const std::vector<uint8_t> msg;
    std::vector<uint8_t> out;

    CHECK(c.Encode(msg, out));
    CHECK(out.empty());
}

TEST_CASE("RawCodec: Encode 覆盖已有的 out 内容") {
    RawCodec c;

    const std::vector<uint8_t> msg = {'a', 'b'};
    std::vector<uint8_t> out  = {'x', 'y', 'z', 'z', 'z'};

    CHECK(c.Encode(msg, out));
    CHECK(out == msg);
}

TEST_CASE("RawCodec: Encode 二进制字节（含 0x00 / 0xFF）") {
    RawCodec c;

    const std::vector<uint8_t> msg = {0x00, 0xFF, 0x01, 0x80, 0x7F};
    std::vector<uint8_t> out;

    CHECK(c.Encode(msg, out));
    CHECK(out == msg);
}

TEST_CASE("RawCodec: Feed 一次性吐出全部字节") {
    RawCodec c;

    int callCount = 0;
    std::vector<uint8_t> received;
    auto onMsg = [&](const std::vector<uint8_t>& m) {
        ++callCount;
        received = m;
    };

    const std::string data = "hello world";
    c.Feed(reinterpret_cast<const uint8_t*>(data.data()), data.size(), onMsg);

    CHECK(callCount == 1);
    CHECK(ToStr(received) == "hello world");
}

TEST_CASE("RawCodec: Feed 多次调用每次都独立触发（不累积）") {
    RawCodec c;

    std::vector<std::string> msgs;
    auto onMsg = [&](const std::vector<uint8_t>& m) { msgs.push_back(ToStr(m)); };

    c.Feed(reinterpret_cast<const uint8_t*>("abc"), 3, onMsg);
    c.Feed(reinterpret_cast<const uint8_t*>("de"),  2, onMsg);
    c.Feed(reinterpret_cast<const uint8_t*>("fghi"), 4, onMsg);

    REQUIRE(msgs.size() == 3);
    CHECK(msgs[0] == "abc");
    CHECK(msgs[1] == "de");
    CHECK(msgs[2] == "fghi");
}

TEST_CASE("RawCodec: Feed len=0 不触发回调") {
    RawCodec c;

    int callCount = 0;
    auto onMsg = [&](const std::vector<uint8_t>&) { ++callCount; };

    c.Feed(reinterpret_cast<const uint8_t*>(""), 0, onMsg);

    CHECK(callCount == 0);
}

TEST_CASE("RawCodec: Feed 单字节也触发") {
    RawCodec c;

    int callCount = 0;
    uint8_t last = 0;
    auto onMsg = [&](const std::vector<uint8_t>& m) {
        ++callCount;
        if (!m.empty()) last = m[0];
    };

    const uint8_t byte = 0x42;
    c.Feed(&byte, 1, onMsg);

    CHECK(callCount == 1);
    CHECK(last == 0x42);
}

TEST_CASE("RawCodec: Feed 二进制数据（含 0x00）") {
    RawCodec c;

    std::vector<uint8_t> received;
    auto onMsg = [&](const std::vector<uint8_t>& m) { received = m; };

    const uint8_t buf[] = {0x00, 0xFF, 0x00, 0x7F, 0x80};
    c.Feed(buf, sizeof(buf), onMsg);

    REQUIRE(received.size() == sizeof(buf));
    CHECK(received[0] == 0x00);
    CHECK(received[1] == 0xFF);
    CHECK(received[2] == 0x00);
    CHECK(received[3] == 0x7F);
    CHECK(received[4] == 0x80);
}

TEST_CASE("RawCodec: Reset 无副作用，后续 Feed 正常") {
    RawCodec c;

    std::string received;
    auto onMsg = [&](const std::vector<uint8_t>& m) { received = ToStr(m); };

    c.Feed(reinterpret_cast<const uint8_t*>("before"), 6, onMsg);
    CHECK(received == "before");

    c.Reset();
    c.Feed(reinterpret_cast<const uint8_t*>("after"), 5, onMsg);
    CHECK(received == "after");
}

TEST_CASE("RawCodec: Reset 连续调用无副作用") {
    RawCodec c;
    c.Reset();
    c.Reset();
    c.Reset();

    std::string received;
    c.Feed(reinterpret_cast<const uint8_t*>("ok"), 2,
           [&](const std::vector<uint8_t>& m) { received = ToStr(m); });

    CHECK(received == "ok");
}

TEST_CASE("RawCodec: Encode → Feed 往返一致") {
    RawCodec c;

    const std::vector<uint8_t> original = {'r', 'o', 'u', 'n', 'd'};
    std::vector<uint8_t> encoded;
    REQUIRE(c.Encode(original, encoded));

    std::vector<uint8_t> decoded;
    c.Feed(encoded.data(), encoded.size(),
           [&](const std::vector<uint8_t>& m) { decoded = m; });

    CHECK(decoded == original);
}
