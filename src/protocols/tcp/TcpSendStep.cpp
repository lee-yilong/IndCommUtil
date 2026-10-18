#include "TcpSendStep.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <regex>

#include "core/StreamSession.h"
#include "log/Log.h"

namespace icu {

TcpSendStep::TcpSendStep(const nlohmann::json& params) {
    m_name = params.value("name", "TCP Send");
    m_variableName = params.value("variable", "");  // ★ 只认变量名
    m_waitMs = params.value("wait_ms", 1000);
    if (params.contains("assert")) m_regexPattern = params["assert"].value("regex", "");
}

std::string TcpSendStep::Name() const {
    return m_name;
}

TestResult TcpSendStep::Execute(ISession* session, std::map<std::string, CommVariable>& vars) {
    TestResult result;
    auto streamSession = dynamic_cast<StreamSession*>(session);
    if (!streamSession) {
        result.errorMessage = "Not a stream session";
        return result;
    }

    // 1. 查找变量
    if (vars.find(m_variableName) == vars.end()) {
        result.errorMessage = "Variable not found: " + m_variableName;
        return result;
    }
    auto& var = vars[m_variableName];

    if (var.access != "RW" && var.access != "WO") {
        result.errorMessage = "Variable is RO, cannot send.";
        return result;
    }

    // 2. 从变量参数中提取 payload
    std::string payload = var.params.value("payload", "");
    if (payload.empty()) {
        result.errorMessage = "Variable payload is empty.";
        return result;
    }

    // 3. 执行发送和等待逻辑
    std::string receivedData;
    std::mutex mtx;
    std::condition_variable cv;
    bool dataReceived = false;

    streamSession->OnRawData([&](int clientId, const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(mtx);
        receivedData.append(reinterpret_cast<const char*>(data), len);
        dataReceived = true;
        cv.notify_all();
    });

    bool sendSuccess =
        streamSession->SendRaw(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    if (!sendSuccess) {
        result.errorMessage = "Failed to send payload";
        return result;
    }

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::milliseconds(m_waitMs), [&]() { return dataReceived; });

    result.success = true;
    result.rawResponse = receivedData;
    return result;
}

bool TcpSendStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    if (m_regexPattern.empty()) return true;
    try {
        std::regex re(m_regexPattern);
        if (std::regex_search(result.rawResponse, re)) return true;
        outErrorMsg = "Response does not match regex.";
        return false;
    } catch (const std::regex_error& e) {
        outErrorMsg = "Invalid regex pattern.";
        return false;
    }
}

void TcpSendStep::Extract(const TestResult& result, std::map<std::string, CommVariable>& vars) {
    if (vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = result.rawResponse;
    }
}

}  // namespace icu
