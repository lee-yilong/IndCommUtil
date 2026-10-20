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
    connOpts.set_automatic_reconnect(true);  // Paho 内置重连

    try {
        m_client.connect(connOpts)->wait();
        // 连接成功会在 connected 回调中处理状态
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
    // 默认订阅所有主题以便接收
    m_client.subscribe("#", 0);
}

void MqttSession::connection_lost(const std::string& cause) {
    m_stateMachine.TransitionTo(SessionState::Reconnecting);
    if (!cause.empty()) {
        ICU_SPDLOG_WARN(_("MQTT Connection lost: %s"), wxString::FromUTF8(cause));
    }
}

void MqttSession::message_arrived(mqtt::const_message_ptr msg) {
    if (m_mqttMsgCb) {
        // 必须在主线程更新 UI
        auto topic = msg->get_topic();
        auto payload = msg->to_string();
        auto cb = m_mqttMsgCb;
        wxTheApp->CallAfter([cb, topic, payload]() { cb(topic, payload); });
    }
}

}  // namespace icu
