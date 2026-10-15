#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace icu {

enum class TransportState { Idle, Opening, Reconnecting, Open, Closed };

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual TransportState State() const = 0;
    virtual bool IsOpen() const = 0;
    virtual std::string Target() const = 0;

    virtual size_t Send(const uint8_t* data, size_t len) = 0;
    virtual int ClientCount() const { return 0; }

    // 服务器模式：true（监听多个客户端）
    // 客户端模式：false（连接单个远端）
    virtual bool IsServerMode() const { return false; }
    // 传输层显示名（UI 用于标识当前传输类型，如 "TCP Client" / "TCP Server" / "Serial"）
    // 由具体 Transport 实现；UI 层通过 wxGetTranslation 翻译
    virtual std::string Name() const { return "Transport"; }

    // ---------- 回调类型 ----------
    using StateCallback = std::function<void(TransportState)>;
    using DataCallback = std::function<void(int clientId, const uint8_t*, size_t)>;
    using ReconnectCallback = std::function<void(int attempt)>;
    using ClientEventCallback = std::function<void(int clientId)>;

    // ---------- 回调注册（public！）----------
    void OnStateChanged(StateCallback cb) { m_stateCb = std::move(cb); }
    void OnDataReceived(DataCallback cb) { m_dataCb = std::move(cb); }
    void OnReconnectAttempt(ReconnectCallback cb) { m_reconnCb = std::move(cb); }
    void OnClientConnected(ClientEventCallback cb) { m_clientConnCb = std::move(cb); }
    void OnClientDisconnected(ClientEventCallback cb) { m_clientDiscCb = std::move(cb); }

protected:
    // ---------- 供子类触发 ----------
    void NotifyState(TransportState st) {
        if (m_stateCb) m_stateCb(st);
    }

    void NotifyData(int clientId, const uint8_t* d, size_t n) {
        if (m_dataCb) m_dataCb(clientId, d, n);
    }

    void NotifyReconnectAttempt(int attempt) {
        if (m_reconnCb) m_reconnCb(attempt);
    }

    void NotifyClientConnected(int id) {
        if (m_clientConnCb) m_clientConnCb(id);
    }

    void NotifyClientDisconnected(int id) {
        if (m_clientDiscCb) m_clientDiscCb(id);
    }

private:
    // ---------- 回调存储 ----------
    StateCallback m_stateCb;
    DataCallback m_dataCb;
    ReconnectCallback m_reconnCb;
    ClientEventCallback m_clientConnCb;
    ClientEventCallback m_clientDiscCb;
};

using ITransportPtr = std::unique_ptr<ITransport>;

}  // namespace icu
