#include "SessionManager.h"

namespace icu {

void SessionManager::SetActive(ISessionPtr s) {
    DetachAndClose();
    m_active = std::move(s);
}

void SessionManager::Close() {
    DetachAndClose();
}

bool SessionManager::IsConnected() const {
    return m_active && m_active->IsConnected();
}

SessionState SessionManager::ActiveState() const {
    return m_active ? m_active->State() : SessionState::Disconnected;
}

void SessionManager::DetachAndClose() {
    if (!m_active) return;

    // 统一调用接口清空所有回调
    m_active->DetachAllCallbacks();
    m_active->Close();
    m_active.reset();
}

}  // namespace icu
