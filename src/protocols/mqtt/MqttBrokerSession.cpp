#include "MqttBrokerSession.h"

#include <wx/app.h>
#include <wx/string.h>

#include "log/Log.h"

namespace icu {

MqttBrokerSession::MqttBrokerSession(const MqttBrokerConfig& cfg) : m_cfg(cfg) {}

MqttBrokerSession::~MqttBrokerSession() {
    Close();
}

std::string MqttBrokerSession::Target() const {
    return "Broker @ 0.0.0.0:" + std::to_string(m_cfg.listenPort);
}

bool MqttBrokerSession::Open() {
    if (IsConnected()) return true;

    m_stateMachine.TransitionTo(SessionState::Connecting);

    // 由于当前暂无内嵌的轻量级 C++ Broker 库，这里作为占位。
    // 如果后续引入了 mosquitto 等库，在此处初始化并启动监听。
    ICU_SPDLOG_WARN(
        _("MQTT Broker mode is currently a placeholder. No actual server is listening."));

    // 假装连接成功，以便 UI 不会崩溃并显示状态
    m_stateMachine.TransitionTo(SessionState::Connected);
    return true;
}

void MqttBrokerSession::Close() {
    m_stateMachine.TransitionTo(SessionState::Disconnected);
}

}  // namespace icu
