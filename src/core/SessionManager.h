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
    SessionPtr m_active;
};

}  // namespace icu
