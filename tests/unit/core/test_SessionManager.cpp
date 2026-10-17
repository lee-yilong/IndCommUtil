#include <memory>

#include "core/SessionManager.h"
#include "core/StreamSession.h"
#include "doctest.h"
#include "mocks/MockCodec.h"
#include "mocks/MockTransport.h"

using namespace icu;
using icu::test::MockCodec;
using icu::test::MockTransport;

namespace {

struct MockSession {
    MockTransport* transport = nullptr;
    StreamSession* raw = nullptr;
    std::shared_ptr<int> closeCount;
    std::shared_ptr<int> openCount;
    std::unique_ptr<StreamSession> session;

    MockSession() {
        auto t = std::make_unique<MockTransport>();
        transport = t.get();
        closeCount = t->closeCount;
        openCount = t->openCount;

        auto c = std::make_unique<MockCodec>();
        session = std::make_unique<StreamSession>(std::move(t), std::move(c));
        raw = session.get();
    }

    // 返回 ISessionPtr 以满足 SetActive 的签名
    ISessionPtr Release() { return std::move(session); }
};

}  // namespace

TEST_CASE("SessionManager: 默认无活动会话") {
    SessionManager mgr;

    CHECK(mgr.Active() == nullptr);
    CHECK_FALSE(mgr.IsConnected());
    CHECK(mgr.ActiveState() == SessionState::Disconnected);
}

TEST_CASE("SessionManager: SetActive 保存会话") {
    SessionManager mgr;
    MockSession ms;
    StreamSession* raw = ms.raw;

    mgr.SetActive(ms.Release());

    CHECK(mgr.Active() == raw);
    CHECK(mgr.ActiveState() == SessionState::Idle);
    CHECK_FALSE(mgr.IsConnected());
}

TEST_CASE("SessionManager: SetActive 替换时关闭旧会话") {
    SessionManager mgr;

    MockSession first;
    MockSession second;

    mgr.SetActive(first.Release());
    CHECK(*first.closeCount == 0);

    mgr.SetActive(second.Release());
    CHECK(*first.closeCount == 1);
    CHECK(mgr.Active() == second.raw);
}

TEST_CASE("SessionManager: SetActive(nullptr) 清空并关闭旧会话") {
    SessionManager mgr;
    MockSession ms;

    mgr.SetActive(ms.Release());
    CHECK(mgr.Active() != nullptr);
    CHECK(*ms.closeCount == 0);

    mgr.SetActive(nullptr);

    CHECK(mgr.Active() == nullptr);
    CHECK_FALSE(mgr.IsConnected());
    CHECK(*ms.closeCount == 1);
}

TEST_CASE("SessionManager: 连续 SetActive 只保留最后一个") {
    SessionManager mgr;

    MockSession s1, s2, s3;
    StreamSession* s3raw = s3.raw;

    mgr.SetActive(s1.Release());
    mgr.SetActive(s2.Release());
    mgr.SetActive(s3.Release());

    CHECK(*s1.closeCount == 1);
    CHECK(*s2.closeCount == 1);
    CHECK(*s3.closeCount == 0);
    CHECK(mgr.Active() == s3raw);
}

TEST_CASE("SessionManager: Close 关闭并清空活动会话") {
    SessionManager mgr;
    MockSession ms;

    mgr.SetActive(ms.Release());
    mgr.Close();

    CHECK(mgr.Active() == nullptr);
    CHECK_FALSE(mgr.IsConnected());
    CHECK(mgr.ActiveState() == SessionState::Disconnected);
    CHECK(*ms.closeCount == 1);
}

TEST_CASE("SessionManager: Close 幂等（无会话 / 重复调用均安全）") {
    SessionManager mgr;

    mgr.Close();
    CHECK(mgr.Active() == nullptr);

    MockSession ms;
    mgr.SetActive(ms.Release());
    mgr.Close();
    mgr.Close();

    CHECK(mgr.Active() == nullptr);
    CHECK(*ms.closeCount == 1);
}

TEST_CASE("SessionManager: ActiveState 跟随 Session 状态变化") {
    SessionManager mgr;
    MockSession ms;
    MockTransport* t = ms.transport;

    mgr.SetActive(ms.Release());
    CHECK(mgr.ActiveState() == SessionState::Idle);

    mgr.Active()->Open();
    CHECK(mgr.ActiveState() == SessionState::Connecting);

    t->SimulateState(TransportState::Open);
    CHECK(mgr.ActiveState() == SessionState::Connected);
    CHECK(mgr.IsConnected());

    t->SimulateState(TransportState::Closed);
    CHECK(mgr.ActiveState() == SessionState::Disconnected);
    CHECK_FALSE(mgr.IsConnected());
}

TEST_CASE("SessionManager: IsConnected 只在 Session Connected 时为真") {
    SessionManager mgr;
    MockSession ms;
    MockTransport* t = ms.transport;

    mgr.SetActive(ms.Release());
    CHECK_FALSE(mgr.IsConnected());

    mgr.Active()->Open();
    CHECK_FALSE(mgr.IsConnected());

    t->SimulateState(TransportState::Open);
    CHECK(mgr.IsConnected());

    t->SimulateState(TransportState::Reconnecting);
    CHECK_FALSE(mgr.IsConnected());

    t->SimulateState(TransportState::Open);
    CHECK(mgr.IsConnected());

    t->SimulateState(TransportState::Closed);
    CHECK_FALSE(mgr.IsConnected());
}
