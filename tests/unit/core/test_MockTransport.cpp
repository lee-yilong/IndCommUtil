#include "doctest.h"
#include "mocks/MockCodec.h"
#include "mocks/MockTransport.h"

#include <string>
#include <vector>

using namespace icu;
using icu::test::MockCodec;
using icu::test::MockTransport;

// =========================================================================
// MockTransport
// =========================================================================

TEST_CASE("MockTransport: 初始状态 Idle") {
    MockTransport t;
    CHECK(t.State() == TransportState::Idle);
    CHECK_FALSE(t.IsOpen());
    CHECK_FALSE(t.IsServerMode());
    CHECK(t.OpenCallCount() == 0);
    CHECK(t.CloseCallCount() == 0);
    CHECK(t.SentBytes().empty());
}

TEST_CASE("MockTransport: Open 切到 Opening 并触发状态回调") {
    MockTransport t;

    int stateCallCount = 0;
    TransportState lastState = TransportState::Idle;
    t.OnStateChanged([&](TransportState st) {
        ++stateCallCount;
        lastState = st;
    });

    CHECK(t.Open());
    CHECK_FALSE(t.IsOpen());
    CHECK(t.State() == TransportState::Opening);
    CHECK(t.OpenCallCount() == 1);
    CHECK(stateCallCount == 1);
    CHECK(lastState == TransportState::Opening);
}

TEST_CASE("MockTransport: Open 失败路径") {
    MockTransport t;
    t.SetOpenShouldFail(true);

    CHECK_FALSE(t.Open());
    CHECK(t.State() == TransportState::Idle);
    CHECK(t.OpenCallCount() == 1);
}

TEST_CASE("MockTransport: Send 记录字节") {
    MockTransport t;
    t.Open();

    const std::string msg = "hello";
    std::vector<uint8_t> bytes(msg.begin(), msg.end());

    const size_t sent = t.Send(bytes.data(), bytes.size());
    CHECK(sent == bytes.size());
    CHECK(t.SentBytes() == bytes);
    CHECK(t.SentString() == "hello");
}

TEST_CASE("MockTransport: Send 失败路径") {
    MockTransport t;
    t.Open();
    t.SetFailSend(true);

    std::vector<uint8_t> bytes = {1, 2, 3};
    CHECK(t.Send(bytes.data(), bytes.size()) == 0);
    CHECK(t.SentBytes().empty());
}

TEST_CASE("MockTransport: InjectData 触发数据回调") {
    MockTransport t;

    int cbClientId = -1;
    std::vector<uint8_t> cbData;
    t.OnDataReceived([&](int id, const uint8_t* d, size_t n) {
        cbClientId = id;
        cbData.assign(d, d + n);
    });

    t.InjectData(42, "world");

    CHECK(cbClientId == 42);
    CHECK(cbData == std::vector<uint8_t>{'w', 'o', 'r', 'l', 'd'});
}

TEST_CASE("MockTransport: SimulateState 触发状态回调") {
    MockTransport t;

    std::vector<TransportState> states;
    t.OnStateChanged([&](TransportState st) { states.push_back(st); });

    t.SimulateState(TransportState::Opening);
    t.SimulateState(TransportState::Open);
    t.SimulateState(TransportState::Reconnecting);
    t.SimulateState(TransportState::Closed);

    REQUIRE(states.size() == 4);
    CHECK(states[0] == TransportState::Opening);
    CHECK(states[1] == TransportState::Open);
    CHECK(states[2] == TransportState::Reconnecting);
    CHECK(states[3] == TransportState::Closed);
}

TEST_CASE("MockTransport: SimulateState 相同状态不重复触发") {
    MockTransport t;

    int count = 0;
    t.OnStateChanged([&](TransportState) { ++count; });

    t.SimulateState(TransportState::Open);
    t.SimulateState(TransportState::Open);
    t.SimulateState(TransportState::Open);

    CHECK(count == 1);
}

TEST_CASE("MockTransport: Close 幂等") {
    MockTransport t;
    t.Open();
    t.Close();
    t.Close();
    t.Close();

    CHECK(t.State() == TransportState::Closed);
    CHECK(t.CloseCallCount() == 3);
}

TEST_CASE("MockTransport: 服务器模式 ClientCount 跟踪") {
    MockTransport t;
    t.SetServerMode(true);
    t.Open();

    CHECK(t.ClientCount() == 0);

    t.SimulateClientConnected(1);
    t.SimulateClientConnected(2);
    CHECK(t.ClientCount() == 2);

    t.SimulateClientDisconnected(1);
    CHECK(t.ClientCount() == 1);
}

TEST_CASE("MockTransport: 客户端事件回调") {
    MockTransport t;

    std::vector<int> connected;
    std::vector<int> disconnected;
    t.OnClientConnected([&](int id) { connected.push_back(id); });
    t.OnClientDisconnected([&](int id) { disconnected.push_back(id); });

    t.SimulateClientConnected(10);
    t.SimulateClientConnected(20);
    t.SimulateClientDisconnected(10);

    REQUIRE(connected.size() == 2);
    CHECK(connected[0] == 10);
    CHECK(connected[1] == 20);
    REQUIRE(disconnected.size() == 1);
    CHECK(disconnected[0] == 10);
}

TEST_CASE("MockTransport: SimulateReconnectAttempt 触发重连回调") {
    MockTransport t;

    std::vector<int> attempts;
    t.OnReconnectAttempt([&](int a) { attempts.push_back(a); });

    t.SimulateReconnectAttempt(1);
    t.SimulateReconnectAttempt(2);
    t.SimulateReconnectAttempt(3);

    CHECK(attempts == std::vector<int>{1, 2, 3});
}

TEST_CASE("MockTransport: SetTarget 影响 Target()") {
    MockTransport t;
    CHECK(t.Target() == "MockTarget");

    t.SetTarget("MockTcp 127.0.0.1:502");
    CHECK(t.Target() == "MockTcp 127.0.0.1:502");
}

// =========================================================================
// MockCodec
// =========================================================================

TEST_CASE("MockCodec: Encode 直通") {
    MockCodec c;
    std::vector<uint8_t> in = {1, 2, 3};
    std::vector<uint8_t> out;

    CHECK(c.Encode(in, out));
    CHECK(out == in);
    CHECK(c.EncodeCount() == 1);
}

TEST_CASE("MockCodec: Encode 失败路径") {
    MockCodec c;
    c.SetFailEncode(true);
    std::vector<uint8_t> in = {1, 2, 3};
    std::vector<uint8_t> out;

    CHECK_FALSE(c.Encode(in, out));
    CHECK(c.EncodeCount() == 1);
}

TEST_CASE("MockCodec: 直通模式一次性吐出全部") {
    MockCodec c;

    int msgCount = 0;
    std::vector<uint8_t> lastMsg;
    auto onMsg = [&](const std::vector<uint8_t>& m) {
        ++msgCount;
        lastMsg = m;
    };

    c.Feed(reinterpret_cast<const uint8_t*>("abc"), 3, onMsg);
    CHECK(msgCount == 1);
    CHECK(lastMsg == std::vector<uint8_t>{'a', 'b', 'c'});

    c.Feed(reinterpret_cast<const uint8_t*>("de"), 2, onMsg);
    CHECK(msgCount == 2);
    CHECK(lastMsg == std::vector<uint8_t>{'d', 'e'});
}

TEST_CASE("MockCodec: 定长模式处理半包") {
    MockCodec c;
    c.SetFrameSize(4);

    int msgCount = 0;
    std::vector<uint8_t> lastMsg;
    auto onMsg = [&](const std::vector<uint8_t>& m) {
        ++msgCount;
        lastMsg = m;
    };

    c.Feed(reinterpret_cast<const uint8_t*>("abc"), 3, onMsg);
    CHECK(msgCount == 0);
    CHECK(c.BufferedBytes() == 3);

    c.Feed(reinterpret_cast<const uint8_t*>("de"), 2, onMsg);
    CHECK(msgCount == 1);
    CHECK(lastMsg == std::vector<uint8_t>{'a', 'b', 'c', 'd'});
    CHECK(c.BufferedBytes() == 1);
}

TEST_CASE("MockCodec: 定长模式处理粘包") {
    MockCodec c;
    c.SetFrameSize(3);

    int msgCount = 0;
    std::vector<std::vector<uint8_t>> msgs;
    auto onMsg = [&](const std::vector<uint8_t>& m) {
        ++msgCount;
        msgs.push_back(m);
    };

    c.Feed(reinterpret_cast<const uint8_t*>("abcdefg"), 7, onMsg);

    CHECK(msgCount == 2);
    CHECK(msgs[0] == std::vector<uint8_t>{'a', 'b', 'c'});
    CHECK(msgs[1] == std::vector<uint8_t>{'d', 'e', 'f'});
    CHECK(c.BufferedBytes() == 1);
}

TEST_CASE("MockCodec: Reset 清空缓冲") {
    MockCodec c;
    c.SetFrameSize(4);
    c.Feed(reinterpret_cast<const uint8_t*>("ab"), 2, [](const std::vector<uint8_t>&) {});

    CHECK(c.BufferedBytes() == 2);

    c.Reset();
    CHECK(c.BufferedBytes() == 0);
    CHECK(c.ResetCount() == 1);
}
