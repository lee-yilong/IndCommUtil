#include "ModbusSession.h"

#include <wx/app.h>
#include <wx/string.h>
#include <wx/tokenzr.h>
#include <wx/translation.h>

#include <chrono>

#include "log/Log.h"
#include "spdlog/spdlog.h"

namespace icu {

ModbusSession::ModbusSession(const ModbusConfig& cfg) : m_cfg(cfg) {}

ModbusSession::~ModbusSession() {
    Close();
}

bool ModbusSession::ConnectPort() {
    if (m_port) {
        delete m_port;
        m_port = nullptr;
    }

    if (m_cfg.modeIndex == 0) {  // TCP
        Modbus::NetSettings settings;
        settings.host = m_cfg.host.c_str();
        settings.port = m_cfg.port;
        settings.timeout = m_cfg.timeoutMs;

        m_port = Modbus::createClientPort(Modbus::TCP, &settings, true);
    }
    return m_port != nullptr;
}

bool ModbusSession::Open() {
    if (m_stateMachine.State() == SessionState::Connected ||
        m_stateMachine.State() == SessionState::Connecting)
        return true;

    m_stateMachine.TransitionTo(SessionState::Connecting);

    m_stopHeartbeat = false;
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    m_heartbeatThread = std::thread(&ModbusSession::HeartbeatLoop, this);

    return true;
}

void ModbusSession::Close() {
    m_stopHeartbeat = true;
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();

    if (m_stateMachine.State() == SessionState::Disconnected) return;

    std::lock_guard<std::mutex> lock(m_portMutex);
    if (m_port) {
        delete m_port;
        m_port = nullptr;
    }
    m_stateMachine.TransitionTo(SessionState::Disconnected);
}

void ModbusSession::HeartbeatLoop() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (!m_stopHeartbeat.load()) {
        bool isConnected = false;

        {
            std::lock_guard<std::mutex> lock(m_portMutex);
            if (!m_port) {
                if (ConnectPort()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }

            if (m_port) {
                uint16_t testReg = 0;
                Modbus::StatusCode status =
                    m_port->readHoldingRegisters(m_cfg.slaveId, 0, 1, &testReg);
                if (Modbus::StatusIsGood(status)) {
                    isConnected = true;
                } else {
                    std::string err = m_port->lastErrorText();
                    if (err.find("exception") != std::string::npos ||
                        err.find("Exception") != std::string::npos) {
                        isConnected = true;
                    }
                }
            }
        }

        if (isConnected) {
            m_stateMachine.TransitionTo(SessionState::Connected);

            for (int i = 0; i < 20 && !m_stopHeartbeat.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(m_portMutex);
                if (m_port) {
                    delete m_port;
                    m_port = nullptr;
                }
            }

            if (!m_cfg.reconn.autoReconnect) {
                m_stateMachine.TransitionTo(SessionState::Disconnected);
                break;
            }

            m_stateMachine.TransitionTo(SessionState::Reconnecting);

            int attempts = m_stateMachine.IncrementAttempt();
            if (m_cfg.reconn.maxAttempts > 0 && attempts > m_cfg.reconn.maxAttempts) {
                m_stateMachine.TransitionTo(SessionState::Disconnected);
                if (m_logCb) m_logCb(std::string(_("Reconnect attempts exhausted.").utf8_str()));
                break;
            }

            int delay = m_cfg.reconn.delaySec > 0 ? m_cfg.reconn.delaySec : 3;

            wxTheApp->CallAfter([this, attempts, delay]() {
                std::string msg = "Retrying in " + std::to_string(delay) + "s (attempt " +
                                  std::to_string(attempts) + ")...";
                if (m_logCb) m_logCb(msg);
            });

            for (int i = 0; i < delay * 10 && !m_stopHeartbeat.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
}

TransactionResponse ModbusSession::Execute(const TransactionRequest& request) {
    TransactionResponse resp;
    std::lock_guard<std::mutex> lock(m_portMutex);
    if (!m_port) {
        resp.errorMessage = "Not connected (waiting for reconnect)";
        return resp;
    }

    Modbus::StatusCode status = Modbus::Status_Good;

    switch (request.functionCode) {
        case 1: {
            std::vector<uint8_t> c(request.count);
            status =
                m_port->readCoils(request.slaveId, request.startAddress, request.count, c.data());
            if (Modbus::StatusIsGood(status)) {
                resp.ok = true;
                for (auto v : c) resp.coils.push_back(v != 0);
            }
            break;
        }
        case 2: {
            std::vector<uint8_t> i(request.count);
            status = m_port->readDiscreteInputs(request.slaveId, request.startAddress,
                                                request.count, i.data());
            if (Modbus::StatusIsGood(status)) {
                resp.ok = true;
                for (auto v : i) resp.coils.push_back(v != 0);
            }
            break;
        }
        case 3: {
            std::vector<uint16_t> r(request.count);
            status = m_port->readHoldingRegisters(request.slaveId, request.startAddress,
                                                  request.count, r.data());
            if (Modbus::StatusIsGood(status)) {
                resp.ok = true;
                resp.registers = std::move(r);
            }
            break;
        }
        case 4: {
            std::vector<uint16_t> r(request.count);
            status = m_port->readInputRegisters(request.slaveId, request.startAddress,
                                                request.count, r.data());
            if (Modbus::StatusIsGood(status)) {
                resp.ok = true;
                resp.registers = std::move(r);
            }
            break;
        }
        case 5: {
            bool v = !request.writeValues.empty() && request.writeValues[0] != 0;
            status = m_port->writeSingleCoil(request.slaveId, request.startAddress, v);
            if (Modbus::StatusIsGood(status)) resp.ok = true;
            break;
        }
        case 6: {
            uint16_t v = request.writeValues.empty() ? 0 : request.writeValues[0];
            status = m_port->writeSingleRegister(request.slaveId, request.startAddress, v);
            if (Modbus::StatusIsGood(status)) resp.ok = true;
            break;
        }
        case 15: {
            std::vector<uint8_t> c;
            for (auto v : request.writeValues) c.push_back(v != 0 ? 1 : 0);
            status = m_port->writeMultipleCoils(request.slaveId, request.startAddress,
                                                static_cast<uint16_t>(c.size()), c.data());
            if (Modbus::StatusIsGood(status)) resp.ok = true;
            break;
        }
        case 16: {
            status = m_port->writeMultipleRegisters(
                request.slaveId, request.startAddress,
                static_cast<uint16_t>(request.writeValues.size()), request.writeValues.data());
            if (Modbus::StatusIsGood(status)) resp.ok = true;
            break;
        }
        default:
            resp.errorMessage = "Unsupported function code";
            return resp;
    }

    if (!resp.ok) {
        if (resp.errorMessage.empty()) resp.errorMessage = m_port->lastErrorText();
        std::string err = resp.errorMessage;
        if (err.find("exception") == std::string::npos &&
            err.find("Exception") == std::string::npos) {
            delete m_port;
            m_port = nullptr;
        }
    }
    return resp;
}

TransactionResponse ModbusSession::Execute(const std::map<std::string, FieldValue>& fields) {
    TransactionRequest req;
    auto itSlave = fields.find("slaveId");
    if (itSlave != fields.end()) req.slaveId = itSlave->second.AsInt();
    auto itFunc = fields.find("functionCode");
    if (itFunc != fields.end()) {
        if (itFunc->second.GetType() == FieldValue::Type::Int)
            req.functionCode = itFunc->second.AsInt();
        else
            req.functionCode = std::stoi(itFunc->second.AsString());
    }
    auto itAddr = fields.find("startAddress");
    if (itAddr != fields.end()) req.startAddress = itAddr->second.AsInt();
    auto itCount = fields.find("count");
    if (itCount != fields.end()) req.count = itCount->second.AsInt();
    auto itWrite = fields.find("writeValues");
    if (itWrite != fields.end() && !itWrite->second.AsString().empty()) {
        wxString str = wxString::FromUTF8(itWrite->second.AsString());
        wxStringTokenizer tokenizer(str, ",");
        while (tokenizer.HasMoreTokens()) {
            long val = 0;
            if (tokenizer.GetNextToken().Trim(true).Trim(false).ToLong(&val))
                req.writeValues.push_back(static_cast<uint16_t>(val));
        }
    }
    return Execute(req);
}

std::string ModbusSession::Target() const {
    if (m_cfg.modeIndex == 0) return "Modbus TCP " + m_cfg.host + ":" + std::to_string(m_cfg.port);
    return "Modbus RTU " + m_cfg.portName + " @ " + std::to_string(m_cfg.baudRate);
}

}  // namespace icu
