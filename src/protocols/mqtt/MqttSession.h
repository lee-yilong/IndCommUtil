#pragma once

#include <mqtt/async_client.h>

#include <functional>
#include <string>

#include "core/ISession.h"
#include "core/SessionStateMachine.h"

namespace icu {

struct MqttConfig {
    std::string brokerHost;
    int brokerPort = 1883;
    std::string clientId;
    int keepAlive = 60;
};

class MqttSession : public ISession, public virtual mqtt::callback {
public:
    explicit MqttSession(const MqttConfig& cfg);
    ~MqttSession() override;

    bool Open() override;
    void Close() override;
    SessionState State() const override { return m_stateMachine.State(); }
    bool IsConnected() const override { return State() == SessionState::Connected; }

    std::string Target() const override;
    std::string TransportName() const override { return "MQTT"; }
    bool IsServerMode() const override { return false; }

    void OnStateChanged(StateCallback cb) override {
        m_stateCb = std::move(cb);
        m_stateMachine.SetCallbacks(m_stateCb, nullptr);
    }
    void DetachAllCallbacks() override {
        m_stateCb = nullptr;
        m_mqttMsgCb = nullptr;
    }

    // MQTT 特有接口
    void Publish(const std::string& topic, const std::string& payload, int qos);

    using MqttMsgCallback =
        std::function<void(const std::string& topic, const std::string& payload)>;
    void OnMqttMessage(MqttMsgCallback cb) { m_mqttMsgCb = std::move(cb); }

    // mqtt::callback 接口
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;

private:
    MqttConfig m_cfg;
    mqtt::async_client m_client;
    SessionStateMachine m_stateMachine;
    StateCallback m_stateCb;
    MqttMsgCallback m_mqttMsgCb;
};

}  // namespace icu
