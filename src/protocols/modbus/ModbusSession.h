#pragma once

#include <ModbusClientPort.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/ITransactionSession.h"
#include "core/ReconnectConfig.h"
#include "core/SessionStateMachine.h"

namespace icu {

struct ModbusConfig {
    int modeIndex = 0;
    std::string role = "Client";
    int slaveId = 1;
    int timeoutMs = 1000;

    ReconnectConfig reconn;  // 使用统一重连配置

    std::string host;
    int port = 502;
    std::string portName;
    int baudRate = 9600;
    std::string parity = "N";
};

class ModbusSession : public ITransactionSession {
public:
    explicit ModbusSession(const ModbusConfig& cfg);
    ~ModbusSession() override;

    bool Open() override;
    void Close() override;
    SessionState State() const override { return m_stateMachine.State(); }
    bool IsConnected() const override { return State() == SessionState::Connected; }

    std::string TransportName() const override {
        return (m_cfg.modeIndex == 0) ? "Modbus TCP" : "Modbus RTU";
    }

    TransactionResponse Execute(const TransactionRequest& request) override;
    TransactionResponse Execute(const std::map<std::string, FieldValue>& fields) override;

    std::string Target() const override;

    void OnStateChanged(StateCallback cb) override {
        m_stateCb = std::move(cb);
        m_stateMachine.SetCallbacks(m_stateCb, m_logCb);
    }
    void OnLog(LogCallback cb) override {
        m_logCb = std::move(cb);
        m_stateMachine.SetCallbacks(m_stateCb, m_logCb);
    }

    void DetachAllCallbacks() override {
        m_stateCb = nullptr;
        m_logCb = nullptr;
        m_stateMachine.ClearCallbacks();
    }

private:
    bool ConnectPort();
    void HeartbeatLoop();

    ModbusConfig m_cfg;
    SessionStateMachine m_stateMachine;
    StateCallback m_stateCb;
    LogCallback m_logCb;

    ModbusClientPort* m_port = nullptr;
    std::mutex m_portMutex;

    std::thread m_heartbeatThread;
    std::atomic<bool> m_stopHeartbeat{false};
};

}  // namespace icu
