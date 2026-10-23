#include "UdpTransport.h"

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

UdpTransport::UdpTransport(const UdpConfig& cfg) : m_cfg(cfg) {}

UdpTransport::~UdpTransport() {
    Close();
}

std::string UdpTransport::Target() const {
    if (m_cfg.isServer) {
        return "UDP Server @ " + m_cfg.host + ":" + std::to_string(m_cfg.port);
    }
    return "UDP Client -> " + m_cfg.host + ":" + std::to_string(m_cfg.port);
}

bool UdpTransport::Open() {
    if (m_state == TransportState::Opening || m_state == TransportState::Open) return true;

    SetState(TransportState::Opening);

    wxIPV4address addr;
    if (m_cfg.isServer) {
        if (m_cfg.host.empty() || m_cfg.host == "0.0.0.0") {
            addr.AnyAddress();
        } else {
            addr.Hostname(wxString::FromUTF8(m_cfg.host.c_str()));
        }
        addr.Service(m_cfg.port);
    } else {
        addr.AnyAddress();
        addr.Service(m_cfg.localPort);
    }

    m_socket = new wxDatagramSocket(addr, wxSOCKET_NONE);
    if (!m_socket->IsOk()) {
        ICU_SPDLOG_ERROR(_("Failed to create UDP socket on port %d"), addr.Service());
        m_socket->Destroy();
        m_socket = nullptr;
        SetState(TransportState::Closed);
        return false;
    }

    m_socket->SetEventHandler(*this, UDP_SOCKET_ID);
    m_socket->SetNotify(wxSOCKET_INPUT_FLAG);
    m_socket->Notify(true);

    Bind(wxEVT_SOCKET, &UdpTransport::OnSocketEvent, this, UDP_SOCKET_ID);

    ICU_SPDLOG_INFO(_("UDP socket opened: %s"), wxString::FromUTF8(Target().c_str()));
    SetState(TransportState::Open);
    return true;
}

void UdpTransport::Close() {
    if (m_socket) {
        Unbind(wxEVT_SOCKET, &UdpTransport::OnSocketEvent, this, UDP_SOCKET_ID);
        m_socket->Notify(false);
        m_socket->Destroy();
        m_socket = nullptr;
    }
    m_hasPeer = false;
    SetState(TransportState::Closed);
    SPDLOG_INFO(_("UDP socket closed"));
}

size_t UdpTransport::Send(const uint8_t* data, size_t len) {
    if (m_state != TransportState::Open || !m_socket) return 0;

    wxIPV4address targetAddr;
    if (!m_cfg.isServer) {
        targetAddr.Hostname(wxString::FromUTF8(m_cfg.host.c_str()));
        targetAddr.Service(m_cfg.port);
    } else {
        if (!m_hasPeer) {
            ICU_SPDLOG_WARN(_("UDP Server has no peer to send to yet. Waiting for incoming data."));
            return 0;
        }
        targetAddr = m_lastPeer;
    }

    m_socket->SendTo(targetAddr, data, static_cast<wxUint32>(len));
    return m_socket->LastCount();
}

void UdpTransport::OnSocketEvent(wxSocketEvent& evt) {
    if (evt.GetSocketEvent() == wxSOCKET_INPUT) {
        uint8_t buf[4096];
        wxIPV4address peer;

        m_socket->RecvFrom(peer, buf, sizeof(buf));
        size_t n = m_socket->LastCount();

        if (n > 0) {
            if (m_cfg.isServer) {
                m_lastPeer = peer;
                m_hasPeer = true;
            }
            NotifyData(0, buf, n);
        }
    }
}

void UdpTransport::SetState(TransportState st) {
    if (m_state == st) return;
    m_state = st;
    NotifyState(st);
}

}  // namespace icu
