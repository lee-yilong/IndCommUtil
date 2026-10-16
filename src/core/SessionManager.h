#pragma once

#include "Session.h"

namespace icu {

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    void SetActive(SessionPtr s);
    void Close();

    bool IsConnected() const;
    Session* Active() const { return m_active.get(); }
    SessionState ActiveState() const;

private:
    // 在 Close() / 析构之前清空 Session 的所有回调，
    // 防止 Session 析构时触发状态变化并 CallAfter 到主循环，
    // 事件在 Session 死后执行会访问悬垂指针（崩溃）。
    void DetachAndClose();

    SessionPtr m_active;
};

}  // namespace icu
