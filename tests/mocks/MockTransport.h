#pragma once

#include "core/ITransport.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace icu {
namespace test {

class MockTransport : public ITransport {
public:
    MockTransport() = default;
    ~MockTransport() override = default;

    bool Open() override {
        ++(*openCount);
        if (m_openShouldFail) return false;
        if (m_state == TransportState::Opening || m_state == TransportState::Open)
            return true;
        m_state = TransportState::Opening;
        NotifyState(m_state);
        return true;
    }

    void Close() override {
        ++(*closeCount);
        if (m_state == TransportState::Closed) return;
        m_state = TransportState::Closed;
        NotifyState(m_state);
    }

    TransportState State() const override { return m_state; }
    bool IsOpen() const override { return m_state == TransportState::Open; }
    bool IsServerMode() const override { return m_serverMode; }
    std::string Target() const override { return m_target; }

    size_t Send(const uint8_t* data, size_t len) override {
        if (len == 0) return 0;
        if (m_failSend) return 0;
        m_sent.insert(m_sent.end(), data, data + len);
        return len;
    }

    int ClientCount() const override {
        return m_serverMode ? static_cast<int>(m_clients.size()) : 0;
    }

    void InjectData(int clientId, const std::vector<uint8_t>& data) {
        if (!data.empty()) NotifyData(clientId, data.data(), data.size());
    }

    void InjectData(int clientId, const std::string& s) {
        InjectData(clientId, std::vector<uint8_t>(s.begin(), s.end()));
    }

    void SimulateState(TransportState st) {
        if (m_state == st) return;
        m_state = st;
        NotifyState(st);
    }

    void SimulateReconnectAttempt(int attempt) {
        NotifyReconnectAttempt(attempt);
    }

    void SimulateClientConnected(int clientId) {
        m_clients.push_back(clientId);
        NotifyClientConnected(clientId);
    }

    void SimulateClientDisconnected(int clientId) {
        auto it = std::find(m_clients.begin(), m_clients.end(), clientId);
        if (it != m_clients.end()) m_clients.erase(it);
        NotifyClientDisconnected(clientId);
    }

    void SetServerMode(bool v) { m_serverMode = v; }
    void SetTarget(const std::string& t) { m_target = t; }
    void SetOpenShouldFail(bool v) { m_openShouldFail = v; }
    void SetFailSend(bool v) { m_failSend = v; }

    const std::vector<uint8_t>& SentBytes() const { return m_sent; }

    std::string SentString() const {
        return std::string(m_sent.begin(), m_sent.end());
    }

    void ClearSent() { m_sent.clear(); }

    int OpenCallCount() const { return *openCount; }
    int CloseCallCount() const { return *closeCount; }

    std::shared_ptr<int> openCount  = std::make_shared<int>(0);
    std::shared_ptr<int> closeCount = std::make_shared<int>(0);

private:
    TransportState m_state = TransportState::Idle;
    bool m_serverMode = false;
    std::string m_target = "MockTarget";

    bool m_openShouldFail = false;
    bool m_failSend = false;

    std::vector<uint8_t> m_sent;
    std::vector<int> m_clients;
};

}  // namespace test
}  // namespace icu
