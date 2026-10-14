#include "TcpTransport.h"

#include <algorithm>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

// =========================================================================
// TcpClientTransport
// =========================================================================

TcpClientTransport::TcpClientTransport(const TcpClientConfig& cfg)
    : m_cfg(cfg), m_connectTimer(this), m_reconnectTimer(this) {
    Bind(wxEVT_TIMER, &TcpClientTransport::OnConnectTimeout, this, m_connectTimer.GetId());
    Bind(wxEVT_TIMER, &TcpClientTransport::OnReconnectTimer, this, m_reconnectTimer.GetId());
}

TcpClientTransport::~TcpClientTransport() {
    Close();
}

std::string TcpClientTransport::Target() const {
    return "Client -> " + m_cfg.remoteHost + ":" + std::to_string(m_cfg.remotePort);
}

bool TcpClientTransport::Open() {
    if (m_state == TransportState::Opening || m_state == TransportState::Reconnecting ||
        m_state == TransportState::Open)
        return true;

    m_reconnectAttempts = 0;
    SetState(TransportState::Opening);
    StartConnect();
    return true;
}

void TcpClientTransport::StartConnect() {
    CleanupSocket();

    m_socket = new wxSocketClient(wxSOCKET_NONE);
    m_socket->SetEventHandler(*this, CLIENT_SOCKET_ID);
    m_socket->SetNotify(wxSOCKET_CONNECTION_FLAG | wxSOCKET_LOST_FLAG | wxSOCKET_INPUT_FLAG);
    m_socket->Notify(true);

    Bind(wxEVT_SOCKET, &TcpClientTransport::OnSocketEvent, this, CLIENT_SOCKET_ID);

    wxIPV4address addr;
    addr.Hostname(wxString::FromUTF8(m_cfg.remoteHost.c_str()));
    addr.Service(m_cfg.remotePort);

    if (m_state == TransportState::Reconnecting) NotifyReconnectAttempt(m_reconnectAttempts);

    ICU_SPDLOG_INFO(_("TCP transport connecting to %s:%d (attempt %d)"),
                    wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort,
                    m_reconnectAttempts);

    m_socket->Connect(addr, false);
    m_connectTimer.StartOnce(m_cfg.connectTimeoutMs);
}

void TcpClientTransport::Close() {
    if (m_connectTimer.IsRunning()) m_connectTimer.Stop();
    if (m_reconnectTimer.IsRunning()) m_reconnectTimer.Stop();
    m_reconnectAttempts = 0;

    CleanupSocket();

    if (m_state != TransportState::Closed) SetState(TransportState::Closed);
}

void TcpClientTransport::CleanupSocket() {
    if (m_socket) {
        Unbind(wxEVT_SOCKET, &TcpClientTransport::OnSocketEvent, this, CLIENT_SOCKET_ID);
        m_socket->Notify(false);
        m_socket->Destroy();
        m_socket = nullptr;
    }
}

size_t TcpClientTransport::Send(const uint8_t* data, size_t len) {
    if (m_state != TransportState::Open || !m_socket) return 0;
    m_socket->Write(data, static_cast<wxUint32>(len));
    return m_socket->LastCount();
}

void TcpClientTransport::SetState(TransportState st) {
    if (m_state == st) return;
    m_state = st;
    NotifyState(st);
}

void TcpClientTransport::OnSocketEvent(wxSocketEvent& evt) {
    switch (evt.GetSocketEvent()) {
        case wxSOCKET_CONNECTION: {
            if (m_connectTimer.IsRunning()) m_connectTimer.Stop();
            if (m_reconnectTimer.IsRunning()) m_reconnectTimer.Stop();
            m_reconnectAttempts = 0;

            ICU_SPDLOG_INFO(_("TCP transport connected to %s:%d"),
                            wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort);
            SetState(TransportState::Open);
            break;
        }

        case wxSOCKET_LOST: {
            if (m_connectTimer.IsRunning()) m_connectTimer.Stop();

            const bool wasOpening = (m_state == TransportState::Opening);
            const bool wasReconnecting = (m_state == TransportState::Reconnecting);
            CleanupSocket();

            if (m_cfg.autoReconnect && ScheduleReconnect()) {
                SetState(TransportState::Reconnecting);
            } else {
                m_reconnectAttempts = 0;
                if (wasOpening)
                    ICU_SPDLOG_WARN(_("TCP transport failed to connect to %s:%d"),
                                    wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort);
                else if (wasReconnecting)
                    ICU_SPDLOG_WARN(_("TCP transport reconnection failed to %s:%d"),
                                    wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort);
                else
                    ICU_SPDLOG_INFO(_("TCP transport disconnected from %s:%d"),
                                    wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort);
                SetState(TransportState::Closed);
            }
            break;
        }

        case wxSOCKET_INPUT: {
            if (!m_socket) break;
            uint8_t buf[4096];
            m_socket->Read(buf, sizeof(buf));
            const size_t n = m_socket->LastCount();
            if (n > 0) NotifyData(0, buf, n);  // 客户端 clientId = 0
            break;
        }

        default:
            break;
    }
}

void TcpClientTransport::OnConnectTimeout(wxTimerEvent&) {
    if (m_state != TransportState::Opening && m_state != TransportState::Reconnecting) return;

    ICU_SPDLOG_WARN(_("TCP transport connect to %s:%d timed out"),
                    wxString::FromUTF8(m_cfg.remoteHost.c_str()), m_cfg.remotePort);
    CleanupSocket();

    if (m_cfg.autoReconnect && ScheduleReconnect()) {
        SetState(TransportState::Reconnecting);
    } else {
        m_reconnectAttempts = 0;
        SetState(TransportState::Closed);
    }
}

void TcpClientTransport::OnReconnectTimer(wxTimerEvent&) {
    if (m_state != TransportState::Reconnecting) return;
    StartConnect();
}

bool TcpClientTransport::ScheduleReconnect() {
    if (!m_cfg.autoReconnect) return false;

    if (m_cfg.reconnectMaxAttempts > 0 && m_reconnectAttempts >= m_cfg.reconnectMaxAttempts) {
        ICU_SPDLOG_WARN(_("Reconnect attempts exhausted (%d)"), m_cfg.reconnectMaxAttempts);
        return false;
    }

    m_reconnectAttempts++;

    int delaySec;
    if (m_cfg.reconnectDelaySec > 0)
        delaySec = m_cfg.reconnectDelaySec;
    else
        delaySec = std::min(kBaseReconnectDelaySec * m_reconnectAttempts, kMaxReconnectDelaySec);

    ICU_SPDLOG_INFO(_("Reconnecting in %d s (attempt %d)"), delaySec, m_reconnectAttempts);

    m_reconnectTimer.StartOnce(delaySec * 1000);
    return true;
}

// =========================================================================
// TcpServerTransport
// =========================================================================

TcpServerTransport::TcpServerTransport(const TcpServerConfig& cfg) : m_cfg(cfg) {}

TcpServerTransport::~TcpServerTransport() {
    Close();
}

std::string TcpServerTransport::Target() const {
    return "Server @ " + m_cfg.listenHost + ":" + std::to_string(m_cfg.listenPort);
}

bool TcpServerTransport::Open() {
    if (m_state == TransportState::Opening || m_state == TransportState::Open) return true;

    SetState(TransportState::Opening);

    wxIPV4address addr;
    if (m_cfg.listenHost.empty() || m_cfg.listenHost == "0.0.0.0")
        addr.AnyAddress();
    else
        addr.Hostname(wxString::FromUTF8(m_cfg.listenHost.c_str()));
    addr.Service(m_cfg.listenPort);

    m_server = new wxSocketServer(addr, wxSOCKET_REUSEADDR);
    if (!m_server->IsOk()) {
        ICU_SPDLOG_ERROR(_("Failed to listen on %s:%d"),
                         wxString::FromUTF8(m_cfg.listenHost.c_str()), m_cfg.listenPort);
        m_server->Destroy();
        m_server = nullptr;
        SetState(TransportState::Closed);
        return false;
    }

    m_server->SetEventHandler(*this, SERVER_SOCKET_ID);
    m_server->SetNotify(wxSOCKET_CONNECTION_FLAG);
    m_server->Notify(true);

    Bind(wxEVT_SOCKET, &TcpServerTransport::OnServerEvent, this, SERVER_SOCKET_ID);

    ICU_SPDLOG_INFO(_("TCP server listening on %s:%d"),
                    wxString::FromUTF8(m_cfg.listenHost.c_str()), m_cfg.listenPort);

    SetState(TransportState::Open);
    return true;
}

void TcpServerTransport::Close() {
    if (m_server) {
        Unbind(wxEVT_SOCKET, &TcpServerTransport::OnServerEvent, this, SERVER_SOCKET_ID);
        m_server->Notify(false);
        m_server->Destroy();
        m_server = nullptr;
    }

    for (auto& kv : m_clients) {
        const int id = kv.first;
        wxSocketBase* sock = kv.second;
        if (sock) {
            Unbind(wxEVT_SOCKET, &TcpServerTransport::OnClientEvent, this, id);
            sock->Notify(false);
            sock->Destroy();
        }
    }
    m_clients.clear();
    m_socketToId.clear();
    m_nextId = 1;

    if (m_state != TransportState::Closed) SetState(TransportState::Closed);
}

size_t TcpServerTransport::Send(const uint8_t* data, size_t len) {
    size_t total = 0;
    for (auto& kv : m_clients) {
        wxSocketBase* sock = kv.second;
        if (sock && sock->IsConnected()) {
            sock->Write(data, static_cast<wxUint32>(len));
            total += sock->LastCount();
        }
    }
    return total;
}

size_t TcpServerTransport::SendTo(int clientId, const uint8_t* data, size_t len) {
    auto it = m_clients.find(clientId);
    if (it == m_clients.end() || !it->second) return 0;
    it->second->Write(data, static_cast<wxUint32>(len));
    return it->second->LastCount();
}

void TcpServerTransport::OnServerEvent(wxSocketEvent& evt) {
    if (evt.GetSocketEvent() == wxSOCKET_CONNECTION) AcceptNewClient();
}

void TcpServerTransport::AcceptNewClient() {
    if (!m_server) return;

    wxSocketBase* sock = m_server->Accept(false);
    if (!sock) return;

    if ((int)m_clients.size() >= m_cfg.maxClients) {
        ICU_SPDLOG_WARN(_("Max clients (%d) reached, rejecting new connection"), m_cfg.maxClients);
        sock->Destroy();
        return;
    }

    const int id = m_nextId++;
    m_clients[id] = sock;
    m_socketToId[sock] = id;

    sock->SetEventHandler(*this, id);
    sock->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
    sock->Notify(true);

    Bind(wxEVT_SOCKET, &TcpServerTransport::OnClientEvent, this, id);

    ICU_SPDLOG_INFO(_("Client #%d connected (total: %d)"), id, (int)m_clients.size());
    NotifyClientConnected(id);
}

void TcpServerTransport::OnClientEvent(wxSocketEvent& evt) {
    const int id = evt.GetId();

    auto it = m_clients.find(id);
    if (it == m_clients.end()) return;
    wxSocketBase* sock = it->second;
    if (!sock) return;

    switch (evt.GetSocketEvent()) {
        case wxSOCKET_INPUT: {
            uint8_t buf[4096];
            sock->Read(buf, sizeof(buf));
            const size_t n = sock->LastCount();
            if (n > 0) NotifyData(id, buf, n);
            break;
        }
        case wxSOCKET_LOST:
            ICU_SPDLOG_INFO(_("Client #%d disconnected (remaining: %d)"), id,
                            (int)m_clients.size() - 1);
            DropClient(sock);
            NotifyClientDisconnected(id);
            break;
        default:
            break;
    }
}

void TcpServerTransport::DropClient(wxSocketBase* sock) {
    auto it = m_socketToId.find(sock);
    if (it == m_socketToId.end()) return;

    const int id = it->second;
    m_socketToId.erase(it);

    auto it2 = m_clients.find(id);
    if (it2 != m_clients.end()) {
        Unbind(wxEVT_SOCKET, &TcpServerTransport::OnClientEvent, this, id);
        it2->second->Notify(false);
        it2->second->Destroy();
        m_clients.erase(it2);
    }
}

void TcpServerTransport::SetState(TransportState st) {
    if (m_state == st) return;
    m_state = st;
    NotifyState(st);
}

}  // namespace icu
