#pragma once

#include <wx/string.h>

#include <atomic>
#include <functional>
#include <string>

#include "core/ISession.h"

namespace icu {

class SessionStateMachine {
public:
    using StateCallback = std::function<void(SessionState)>;
    using LogCallback = std::function<void(const std::string&)>;

    void SetCallbacks(StateCallback stateCb, LogCallback logCb) {
        m_stateCb = std::move(stateCb);
        m_logCb = std::move(logCb);
    }

    void ClearCallbacks() {
        m_stateCb = nullptr;
        m_logCb = nullptr;
    }

    SessionState State() const { return m_state.load(); }

    void TransitionTo(SessionState newState) {
        if (m_state == newState) return;

        SessionState oldState = m_state.exchange(newState);

        if (newState == SessionState::Connected) {
            m_reconnectAttempts = 0;
        }

        if (m_stateCb) m_stateCb(newState);

        if (m_logCb) {
            if (newState == SessionState::Reconnecting) {
                if (oldState == SessionState::Connected)
                    m_logCb(std::string(_("Connection lost.").utf8_str()));
                else
                    m_logCb(std::string(_("Connection failed.").utf8_str()));
            } else if (newState == SessionState::Connected) {
                if (oldState == SessionState::Reconnecting)
                    m_logCb(std::string(_("Reconnected successfully.").utf8_str()));
                else
                    m_logCb(std::string(_("Connected successfully.").utf8_str()));
            }
        }
    }

    int IncrementAttempt() { return ++m_reconnectAttempts; }
    int GetAttempts() const { return m_reconnectAttempts; }
    void ResetAttempts() { m_reconnectAttempts = 0; }

private:
    std::atomic<SessionState> m_state{SessionState::Idle};
    std::atomic<int> m_reconnectAttempts{0};
    StateCallback m_stateCb;
    LogCallback m_logCb;
};

}  // namespace icu
