#include "MqttSession.h"

#include <wx/app.h>
#include <wx/string.h>

#include "log/Log.h"

namespace icu {

MqttSession::MqttSession(const MqttConfig& cfg)
    : m_cfg(cfg),
      m_client("tcp://" + cfg.brokerHost + ":" + std::to_string(cfg.brokerPort), cfg.clientId) {
    m_client.set_callback(*this);
}

MqttSession::~MqttSession() {
    Close();
}

std::string MqttSession::Target() const {
    return "tcp://" + m_cfg.brokerHost + ":" + std::to_string(m_cfg.brokerPort);
}

bool MqttSession::Open() {
    if (IsConnected()) return true;

    m_stateMachine.TransitionTo(SessionState::Connecting);
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(m_cfg.keepAlive);
    connOpts.set_clean_session(true);
    connOpts.set_automatic_reconnect(true);

    try {
        m_client.connect(connOpts)->wait();
        return true;
    } catch (const mqtt::exception& exc) {
        ICU_SPDLOG_ERROR(_("MQTT Connection failed: %s"), wxString::FromUTF8(exc.what()));
        m_stateMachine.TransitionTo(SessionState::Disconnected);
        return false;
    }
}

void MqttSession::Close() {
    try {
        if (m_client.is_connected()) {
            m_client.disconnect()->wait();
        }
    } catch (...) {
    }
    m_stateMachine.TransitionTo(SessionState::Disconnected);
}

void MqttSession::Publish(const std::string& topic, const std::string& payload, int qos) {
    if (!IsConnected()) return;
    if (m_cfg.isSubscriber) {
        ICU_SPDLOG_WARN(_("Cannot publish in Subscriber mode."));
        return;
    }
    try {
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(qos);
        m_client.publish(msg);
    } catch (const mqtt::exception& exc) {
        ICU_SPDLOG_ERROR(_("MQTT Publish failed: %s"), wxString::FromUTF8(exc.what()));
    }
}

void MqttSession::connected(const std::string& /*cause*/) {
    m_stateMachine.TransitionTo(SessionState::Connected);

    if (m_cfg.isSubscriber) {
        try {
            m_client.subscribe(m_cfg.subscribeTopic, m_cfg.subscribeQos);
            ICU_SPDLOG_INFO(_("MQTT Subscribed to: %s"), wxString::FromUTF8(m_cfg.subscribeTopic));
        } catch (const mqtt::exception& exc) {
            ICU_SPDLOG_ERROR(_("MQTT Subscribe failed: %s"), wxString::FromUTF8(exc.what()));
        }
    }
}

void MqttSession::connection_lost(const std::string& cause) {
    m_stateMachine.TransitionTo(SessionState::Reconnecting);
    if (!cause.empty()) {
        ICU_SPDLOG_WARN(_("MQTT Connection lost: %s"), wxString::FromUTF8(cause));
    }
}

void MqttSession::message_arrived(mqtt::const_message_ptr msg) {
    if (m_cfg.isSubscriber && m_mqttMsgCb) {
        auto topic = msg->get_topic();
        auto payload = msg->to_string();
        auto cb = m_mqttMsgCb;
        wxTheApp->CallAfter([cb, topic, payload]() { cb(topic, payload); });
    }
}

}  // namespace icu
