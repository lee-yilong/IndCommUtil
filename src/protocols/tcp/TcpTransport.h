#pragma once

// =========================================================================
// TCP 协议的传输实现
//
//   TcpClientTransport  —— 客户端模式（wxSocketClient），连接单个远端
//   TcpServerTransport  —— 服务器模式（wxSocketServer），监听多个客户端
//
// 两者都实现 ITransport，由 TcpProtocol::CreateSession() 根据 mode 选择
// 创建哪一个。放在同一文件中，使"TCP 协议"在代码组织上是一个整体。
// =========================================================================

#include <wx/event.h>
#include <wx/socket.h>
#include <wx/timer.h>

#include <map>
#include <string>

#include "core/ITransport.h"

namespace icu {

// =========================================================================
// 配置结构
// =========================================================================

struct TcpClientConfig {
    std::string remoteHost;
    int remotePort = 0;
    int connectTimeoutMs = 3000;
    bool autoReconnect = false;
    int reconnectMaxAttempts = 0;  // 0 = 无限
    int reconnectDelaySec = 0;     // 0 = 自动递增
};

struct TcpServerConfig {
    std::string listenHost = "0.0.0.0";
    int listenPort = 502;
    int maxClients = 10;
    int backlog = 5;
};

// =========================================================================
// TcpClientTransport —— TCP 客户端模式
// =========================================================================

class TcpClientTransport : public wxEvtHandler, public ITransport {
public:
    explicit TcpClientTransport(const TcpClientConfig& cfg);
    ~TcpClientTransport() override;

    bool Open() override;
    void Close() override;
    TransportState State() const override { return m_state; }
    bool IsOpen() const override { return m_state == TransportState::Open; }
    bool IsServerMode() const override { return false; }
    std::string Target() const override;

    std::string Name() const override { return "TCP Client"; }

    size_t Send(const uint8_t* data, size_t len) override;

private:
    enum { CLIENT_SOCKET_ID = wxID_HIGHEST + 200 };

    void OnSocketEvent(wxSocketEvent& evt);
    void OnConnectTimeout(wxTimerEvent& evt);
    void OnReconnectTimer(wxTimerEvent& evt);

    void StartConnect();
    bool ScheduleReconnect();
    void SetState(TransportState st);
    void CleanupSocket();

    TcpClientConfig m_cfg;
    wxSocketClient* m_socket = nullptr;
    TransportState m_state = TransportState::Idle;

    wxTimer m_connectTimer;
    wxTimer m_reconnectTimer;
    int m_reconnectAttempts = 0;

    static constexpr int kBaseReconnectDelaySec = 3;
    static constexpr int kMaxReconnectDelaySec = 10;
};

// =========================================================================
// TcpServerTransport —— TCP 服务器模式
// =========================================================================

class TcpServerTransport : public wxEvtHandler, public ITransport {
public:
    explicit TcpServerTransport(const TcpServerConfig& cfg);
    ~TcpServerTransport() override;

    bool Open() override;
    void Close() override;
    TransportState State() const override { return m_state; }
    bool IsOpen() const override { return m_state == TransportState::Open; }
    bool IsServerMode() const override { return true; }
    std::string Target() const override;
    std::string Name() const override { return "TCP Server"; }

    size_t Send(const uint8_t* data, size_t len) override;
    size_t SendTo(int clientId, const uint8_t* data, size_t len);

    int ClientCount() const override { return (int)m_clients.size(); }

private:
    enum { SERVER_SOCKET_ID = wxID_HIGHEST + 100 };

    void OnServerEvent(wxSocketEvent& evt);
    void OnClientEvent(wxSocketEvent& evt);

    void AcceptNewClient();
    void DropClient(wxSocketBase* sock);
    void SetState(TransportState st);

    TcpServerConfig m_cfg;
    wxSocketServer* m_server = nullptr;
    TransportState m_state = TransportState::Idle;

    std::map<int, wxSocketBase*> m_clients;     // id -> socket
    std::map<wxSocketBase*, int> m_socketToId;  // socket -> id
    int m_nextId = 1;
};

}  // namespace icu
