#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/ICodec.h"
#include "core/ISession.h"
#include "core/ITransport.h"

namespace icu {

class StreamSession : public ISession {
public:
    StreamSession(ITransportPtr transport, ICodecPtr codec);
    ~StreamSession() override = default;

    // --- ISession 接口实现 ---
    bool Open() override;
    void Close() override;
    SessionState State() const override { return m_state; }
    bool IsConnected() const override { return m_state == SessionState::Connected; }
    std::string Target() const override;
    std::string TransportName() const override;
    void OnStateChanged(StateCallback cb) override { m_stateCb = std::move(cb); }
    void DetachAllCallbacks() override;

    // --- 字节流特有接口 ---
    bool Send(const std::vector<uint8_t>& message);
    bool SendRaw(const uint8_t* data, size_t len);
    std::string Protocol() const;  // Codec 名字
    bool IsServerMode() const;
    ITransport* Transport() const { return m_transport.get(); }

    using MessageCallback = std::function<void(int clientId, const std::vector<uint8_t>&)>;
    using RawCallback = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    void OnMessage(MessageCallback cb) { m_msgCb = std::move(cb); }
    void OnRawData(RawCallback cb) { m_rawCb = std::move(cb); }
    void OnReconnectAttempt(ReconnectCallback cb) { m_reconnCb = std::move(cb); }
    void OnClientConnected(ClientEventCallback cb) { m_clientConnCb = std::move(cb); }
    void OnClientDisconnected(ClientEventCallback cb) { m_clientDiscCb = std::move(cb); }

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

}  // namespace icu
