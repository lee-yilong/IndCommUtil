#include "SessionManager.h"

namespace icu {

void SessionManager::SetActive(SessionPtr s) {
    Close();
    m_active = std::move(s);
}

void SessionManager::Close() {
    if (m_active) {
        m_active->Close();
        m_active.reset();
    }
}

bool SessionManager::IsConnected() const {
    return m_active && m_active->IsConnected();
}

SessionState SessionManager::ActiveState() const {
    return m_active ? m_active->State() : SessionState::Disconnected;
}

}  // namespace icu
