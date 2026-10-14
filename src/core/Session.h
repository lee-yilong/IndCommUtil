#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/ICodec.h"
#include "core/ITransport.h"

namespace icu {

enum class SessionState { Idle, Connecting, Reconnecting, Connected, Disconnected };

class Session {
public:
    Session(ITransportPtr transport, ICodecPtr codec);

    bool Open();
    void Close();
    SessionState State() const { return m_state; }
    bool IsConnected() const { return m_state == SessionState::Connected; }

    bool Send(const std::vector<uint8_t>& message);
    bool SendRaw(const uint8_t* data, size_t len);

    std::string Target() const;
    std::string Protocol() const;
    std::string TransportName() const;

    using StateCallback = std::function<void(SessionState)>;
    using MessageCallback = std::function<void(int clientId, const std::vector<uint8_t>&)>;
    using RawCallback = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback = std::function<void(int attempt)>;

    void OnStateChanged(StateCallback cb) { m_stateCb = std::move(cb); }
    void OnMessage(MessageCallback cb) { m_msgCb = std::move(cb); }
    void OnRawData(RawCallback cb) { m_rawCb = std::move(cb); }
    void OnReconnectAttempt(ReconnectCallback cb) { m_reconnCb = std::move(cb); }

    ITransport* Transport() const { return m_transport.get(); }

    using ClientEventCallback = std::function<void(int clientId)>;
    void OnClientConnected(ClientEventCallback cb) { m_clientConnCb = std::move(cb); }
    void OnClientDisconnected(ClientEventCallback cb) { m_clientDiscCb = std::move(cb); }

    bool IsServerMode() const;

private:
    void HandleTransportState(TransportState st);
    void HandleTransportData(int clientId, const uint8_t* data, size_t len);

    ITransportPtr m_transport;
    ICodecPtr m_codec;
    SessionState m_state = SessionState::Idle;

    StateCallback m_stateCb;
    MessageCallback m_msgCb;
    RawCallback m_rawCb;
    ReconnectCallback m_reconnCb;
    ClientEventCallback m_clientConnCb;
    ClientEventCallback m_clientDiscCb;
};

using SessionPtr = std::unique_ptr<Session>;

}  // namespace icu
