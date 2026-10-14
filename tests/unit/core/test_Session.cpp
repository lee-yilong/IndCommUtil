#include "core/Session.h"
#include "doctest.h"
#include "mocks/MockCodec.h"
#include "mocks/MockTransport.h"

#include <memory>
#include <string>
#include <vector>

using namespace icu;
using icu::test::MockCodec;
using icu::test::MockTransport;

namespace {

struct Fixture {
    MockTransport* transport = nullptr;
    MockCodec*     codec     = nullptr;
    std::unique_ptr<Session> session;

    Fixture() {
        auto t = std::make_unique<MockTransport>();
        auto c = std::make_unique<MockCodec>();
        transport = t.get();
        codec     = c.get();
        session   = std::make_unique<Session>(std::move(t), std::move(c));
    }
};

}  // namespace

TEST_CASE("Session: 初始状态 Idle") {
    Fixture f;
    CHECK(f.session->State() == SessionState::Idle);
    CHECK_FALSE(f.session->IsConnected());
    CHECK(f.transport->OpenCallCount() == 0);
}

TEST_CASE("Session: Open 触发 Transport::Open") {
    Fixture f;

    CHECK(f.session->Open());
    CHECK(f.transport->OpenCallCount() == 1);
    CHECK(f.codec->ResetCount() >= 1);
}

TEST_CASE("Session: Open 幂等（已连接时不再触发）") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    CHECK(f.session->IsConnected());

    f.session->Open();
    CHECK(f.transport->OpenCallCount() == 1);
}

TEST_CASE("Session: Transport Opening → Session Connecting") {
    Fixture f;

    std::vector<SessionState> states;
    f.session->OnStateChanged([&](SessionState st) { states.push_back(st); });

    f.session->Open();
    f.transport->SimulateState(TransportState::Opening);

    REQUIRE(states.size() == 1);
    CHECK(states[0] == SessionState::Connecting);
    CHECK(f.session->State() == SessionState::Connecting);
}

TEST_CASE("Session: Transport Open → Session Connected") {
    Fixture f;

    std::vector<SessionState> states;
    f.session->OnStateChanged([&](SessionState st) { states.push_back(st); });

    f.session->Open();
    f.transport->SimulateState(TransportState::Opening);
    f.transport->SimulateState(TransportState::Open);

    REQUIRE(states.size() == 2);
    CHECK(states[0] == SessionState::Connecting);
    CHECK(states[1] == SessionState::Connected);
    CHECK(f.session->IsConnected());
}

TEST_CASE("Session: Transport Reconnecting → Session Reconnecting") {
    Fixture f;

    std::vector<SessionState> states;
    f.session->OnStateChanged([&](SessionState st) { states.push_back(st); });

    f.session->Open();
    f.transport->SimulateState(TransportState::Opening);
    f.transport->SimulateState(TransportState::Open);
    f.transport->SimulateState(TransportState::Reconnecting);

    REQUIRE(states.size() == 3);
    CHECK(states[2] == SessionState::Reconnecting);
}

TEST_CASE("Session: Transport Closed → Session Disconnected") {
    Fixture f;

    std::vector<SessionState> states;
    f.session->OnStateChanged([&](SessionState st) { states.push_back(st); });

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    f.transport->SimulateState(TransportState::Closed);

    REQUIRE(states.size() == 3);
    CHECK(states[0] == SessionState::Connecting);
    CHECK(states[1] == SessionState::Connected);
    CHECK(states[2] == SessionState::Disconnected);
    CHECK_FALSE(f.session->IsConnected());
}

TEST_CASE("Session: 相同状态不重复通知") {
    Fixture f;

    int count = 0;
    f.session->OnStateChanged([&](SessionState) { ++count; });

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    f.transport->SimulateState(TransportState::Open);
    f.transport->SimulateState(TransportState::Open);

    CHECK(count == 2);
}

TEST_CASE("Session: 未连接时 Send 返回 false") {
    Fixture f;

    std::vector<uint8_t> msg = {'h', 'i'};
    CHECK_FALSE(f.session->Send(msg));
    CHECK(f.transport->SentBytes().empty());
    CHECK(f.codec->EncodeCount() == 0);
}

TEST_CASE("Session: 连接后 Send 走 Codec → Transport") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);

    std::vector<uint8_t> msg = {'h', 'e', 'l', 'l', 'o'};
    CHECK(f.session->Send(msg));

    CHECK(f.codec->EncodeCount() == 1);
    CHECK(f.transport->SentString() == "hello");
}

TEST_CASE("Session: Send 空消息时 Transport 收到空") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);

    std::vector<uint8_t> empty;
    CHECK(f.session->Send(empty));

    CHECK(f.transport->SentBytes().empty());
}

TEST_CASE("Session: Codec Encode 失败时 Send 返回 false") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    f.codec->SetFailEncode(true);

    std::vector<uint8_t> msg = {'x'};
    CHECK_FALSE(f.session->Send(msg));
    CHECK(f.transport->SentBytes().empty());
}

TEST_CASE("Session: Transport Send 失败时 Send 返回 false") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    f.transport->SetFailSend(true);

    std::vector<uint8_t> msg = {'x', 'y'};
    CHECK_FALSE(f.session->Send(msg));
}

TEST_CASE("Session: 收到数据触发 OnRawData 和 OnMessage") {
    Fixture f;

    std::vector<uint8_t> rawData;
    std::vector<uint8_t> msgData;
    int rawClientId = -1;
    int msgClientId = -1;

    f.session->OnRawData([&](int id, const uint8_t* d, size_t n) {
        rawClientId = id;
        rawData.assign(d, d + n);
    });
    f.session->OnMessage([&](int id, const std::vector<uint8_t>& m) {
        msgClientId = id;
        msgData = m;
    });

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);

    f.transport->InjectData(0, "hello");

    CHECK(rawClientId == 0);
    CHECK(rawData == std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o'});
    CHECK(msgClientId == 0);
    CHECK(msgData == std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o'});
}

TEST_CASE("Session: clientId 透传（服务器模式）") {
    Fixture f;

    int receivedClientId = -1;
    f.session->OnMessage([&](int id, const std::vector<uint8_t>&) {
        receivedClientId = id;
    });

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);

    f.transport->InjectData(42, "from-client-42");

    CHECK(receivedClientId == 42);
}

TEST_CASE("Session: Codec 定长切帧后多次 OnMessage") {
    Fixture f;

    f.codec->SetFrameSize(3);

    std::vector<std::vector<uint8_t>> msgs;
    f.session->OnMessage([&](int, const std::vector<uint8_t>& m) {
        msgs.push_back(m);
    });

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);

    f.transport->InjectData(0, "abcdefg");

    REQUIRE(msgs.size() == 2);
    CHECK(msgs[0] == std::vector<uint8_t>{'a', 'b', 'c'});
    CHECK(msgs[1] == std::vector<uint8_t>{'d', 'e', 'f'});
    CHECK(f.codec->BufferedBytes() == 1);
}

TEST_CASE("Session: Reconnect 事件透传") {
    Fixture f;

    std::vector<int> attempts;
    f.session->OnReconnectAttempt([&](int a) { attempts.push_back(a); });

    f.transport->SimulateReconnectAttempt(1);
    f.transport->SimulateReconnectAttempt(2);

    CHECK(attempts == std::vector<int>{1, 2});
}

TEST_CASE("Session: Client 上下线事件透传") {
    Fixture f;

    std::vector<int> conn;
    std::vector<int> disc;
    f.session->OnClientConnected([&](int id) { conn.push_back(id); });
    f.session->OnClientDisconnected([&](int id) { disc.push_back(id); });

    f.transport->SimulateClientConnected(1);
    f.transport->SimulateClientConnected(2);
    f.transport->SimulateClientDisconnected(1);

    CHECK(conn == std::vector<int>{1, 2});
    CHECK(disc == std::vector<int>{1});
}

TEST_CASE("Session: Target 来自 Transport") {
    Fixture f;
    f.transport->SetTarget("MockTcp 127.0.0.1:5020");
    CHECK(f.session->Target() == "MockTcp 127.0.0.1:5020");
}

TEST_CASE("Session: Protocol 来自 Codec") {
    Fixture f;
    CHECK(f.session->Protocol() == "Mock");
}

TEST_CASE("Session: IsServerMode 透传") {
    Fixture f;
    CHECK_FALSE(f.session->IsServerMode());

    f.transport->SetServerMode(true);
    CHECK(f.session->IsServerMode());
}

TEST_CASE("Session: Close 调用 Transport::Close") {
    Fixture f;

    f.session->Open();
    f.transport->SimulateState(TransportState::Open);
    f.session->Close();

    CHECK(f.transport->CloseCallCount() == 1);
}
