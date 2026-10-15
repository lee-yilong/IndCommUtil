#include "Session.h"

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

Session::Session(ITransportPtr transport, ICodecPtr codec)
    : m_transport(std::move(transport)), m_codec(std::move(codec)) {
    m_transport->OnStateChanged([this](TransportState st) { HandleTransportState(st); });

    m_transport->OnDataReceived(
        [this](int clientId, const uint8_t* d, size_t n) { HandleTransportData(clientId, d, n); });

    m_transport->OnReconnectAttempt([this](int attempt) {
        if (m_reconnCb) m_reconnCb(attempt);
    });

    m_transport->OnClientConnected([this](int id) {
        if (m_clientConnCb) m_clientConnCb(id);
    });
    m_transport->OnClientDisconnected([this](int id) {
        if (m_clientDiscCb) m_clientDiscCb(id);
    });
}

bool Session::Open() {
    if (m_state == SessionState::Connecting || m_state == SessionState::Connected ||
        m_state == SessionState::Reconnecting)
        return true;

    m_codec->Reset();
    return m_transport->Open();
}

void Session::Close() {
    m_transport->Close();
}

bool Session::Send(const std::vector<uint8_t>& message) {
    if (m_state != SessionState::Connected) return false;

    std::vector<uint8_t> bytes;
    if (!m_codec->Encode(message, bytes)) {
        ICU_SPDLOG_WARN(_("Codec '%s' encode failed"), wxString::FromUTF8(m_codec->Name()));
        return false;
    }
    if (bytes.empty()) return true;

    const size_t sent = m_transport->Send(bytes.data(), bytes.size());
    return sent == bytes.size();
}

bool Session::SendRaw(const uint8_t* data, size_t len) {
    if (m_state != SessionState::Connected) return false;
    return m_transport->Send(data, len) == len;
}

std::string Session::Target() const {
    return m_transport->Target();
}
std::string Session::Protocol() const {
    return m_codec->Name();
}

std::string Session::TransportName() const {
    return m_transport->IsServerMode() ? "TCP Server" : "TCP Client";
}

void Session::HandleTransportState(TransportState st) {
    SessionState newState = m_state;
    switch (st) {
        case TransportState::Idle:
            newState = SessionState::Idle;
            break;
        case TransportState::Opening:
            newState = SessionState::Connecting;
            break;
        case TransportState::Reconnecting:
            newState = SessionState::Reconnecting;
            break;
        case TransportState::Open:
            newState = SessionState::Connected;
            break;
        case TransportState::Closed:
            newState = SessionState::Disconnected;
            break;
    }

    if (newState != m_state) {
        m_state = newState;
        if (m_stateCb) m_stateCb(m_state);
    }
}

void Session::HandleTransportData(int clientId, const uint8_t* data, size_t len) {
    if (m_rawCb) m_rawCb(clientId, data, len);

    m_codec->Feed(data, len, [this, clientId](const std::vector<uint8_t>& msg) {
        if (m_msgCb) m_msgCb(clientId, msg);
    });
}

bool Session::IsServerMode() const {
    return m_transport && m_transport->IsServerMode();
}

}  // namespace icu
