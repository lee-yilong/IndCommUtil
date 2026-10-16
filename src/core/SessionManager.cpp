#include "SessionManager.h"

namespace icu {

void SessionManager::SetActive(SessionPtr s) {
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

    // 关键：先清空所有回调，再 Close / 析构。
    // 否则 Session 析构时 transport 触发 NotifyState(Closed) →
    // Session::HandleTransportState → m_stateCb(Disconnected) →
    // MainFrame 的 lambda → CallAfter → 主循环处理时 Session 已死 → 崩溃。
    m_active->OnStateChanged(nullptr);
    m_active->OnMessage(nullptr);
    m_active->OnRawData(nullptr);
    m_active->OnReconnectAttempt(nullptr);
    m_active->OnClientConnected(nullptr);
    m_active->OnClientDisconnected(nullptr);

    m_active->Close();
    m_active.reset();
}

}  // namespace icu
