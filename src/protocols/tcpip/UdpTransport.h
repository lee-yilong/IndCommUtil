#pragma once

#include <wx/event.h>
#include <wx/socket.h>

#include <string>

#include "core/ITransport.h"

namespace icu {

struct UdpConfig {
    bool isServer = false;
    std::string host = "127.0.0.1";
    int port = 5020;
    int localPort = 0;
};

class UdpTransport : public wxEvtHandler, public ITransport {
public:
    explicit UdpTransport(const UdpConfig& cfg);
    ~UdpTransport() override;

    bool Open() override;
    void Close() override;
    TransportState State() const override { return m_state; }
    bool IsOpen() const override { return m_state == TransportState::Open; }
    bool IsServerMode() const override { return m_cfg.isServer; }
    std::string Target() const override;
    std::string Name() const override { return m_cfg.isServer ? "UDP Server" : "UDP Client"; }

    size_t Send(const uint8_t* data, size_t len) override;

private:
    enum { UDP_SOCKET_ID = wxID_HIGHEST + 300 };

    void OnSocketEvent(wxSocketEvent& evt);
    void SetState(TransportState st);

    UdpConfig m_cfg;
    wxDatagramSocket* m_socket = nullptr;
    TransportState m_state = TransportState::Idle;

    wxIPV4address m_lastPeer;
    bool m_hasPeer = false;
};

}  // namespace icu
