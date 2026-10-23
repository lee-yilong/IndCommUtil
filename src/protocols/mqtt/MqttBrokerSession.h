#pragma once

#include <functional>
#include <string>

#include "core/ISession.h"
#include "core/SessionStateMachine.h"

namespace icu {

struct MqttBrokerConfig {
    int listenPort = 1883;
};

class MqttBrokerSession : public ISession {
public:
    explicit MqttBrokerSession(const MqttBrokerConfig& cfg);
    ~MqttBrokerSession() override;

    bool Open() override;
    void Close() override;
    SessionState State() const override { return m_stateMachine.State(); }
    bool IsConnected() const override { return State() == SessionState::Connected; }

    std::string Target() const override;
    std::string TransportName() const override { return "MQTT Broker"; }
    bool IsServerMode() const override { return true; }

    void OnStateChanged(StateCallback cb) override {
        m_stateCb = std::move(cb);
        m_stateMachine.SetCallbacks(m_stateCb, nullptr);
    }
    void DetachAllCallbacks() override { m_stateCb = nullptr; }

private:
    MqttBrokerConfig m_cfg;
    SessionStateMachine m_stateMachine;
    StateCallback m_stateCb;
};

}  // namespace icu
