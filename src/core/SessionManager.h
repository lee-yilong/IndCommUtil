#pragma once

#include "core/ISession.h"

namespace icu {

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    void SetActive(ISessionPtr s);
    void Close();

    bool IsConnected() const;
    ISession* Active() const { return m_active.get(); }
    SessionState ActiveState() const;

private:
    // 统一的销毁安全机制
    void DetachAndClose();

    ISessionPtr m_active;
};

}  // namespace icu
