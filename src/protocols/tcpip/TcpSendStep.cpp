#include "TcpSendStep.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <regex>

#include "core/StreamSession.h"
#include "core/ValueValidator.h"
#include "log/Log.h"

namespace icu {

TcpSendStep::TcpSendStep(const nlohmann::json& params) {
    m_name = params.value("name", "TCP Step");
    m_variableName = params.value("variable", "");
    m_waitMs = params.value("wait_ms", 1000);

    m_isReadOnly = (params.value("action", "") == "read");

    if (params.contains("assert") && params["assert"].is_object()) {
        const auto& a = params["assert"];
        m_assertType = a.value("type", "");
        if (a.contains("expected")) {
            if (a["expected"].is_string())
                m_expectedValue = a["expected"].get<std::string>();
            else if (a["expected"].is_number())
                m_expectedValue = std::to_string(a["expected"].get<double>());
        }
    }
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

    if (vars.find(m_variableName) == vars.end()) {
        result.errorMessage = "Variable not found: " + m_variableName;
        return result;
    }
    auto& var = vars[m_variableName];

    if (!m_isReadOnly && var.access == "RO") {
        result.errorMessage = "Variable is RO, cannot send.";
        return result;
    }

    struct WaitState {
        std::mutex mtx;
        std::condition_variable cv;
        std::string receivedData;
        bool dataReceived = false;
    };
    auto state = std::make_shared<WaitState>();

    streamSession->OnRawData([state](int clientId, const uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(state->mtx);
        state->receivedData.append(reinterpret_cast<const char*>(data), len);
        state->dataReceived = true;
        state->cv.notify_all();
    });

    if (!m_isReadOnly) {
        const std::string payload = var.currentValue;
        if (payload.empty()) {
            streamSession->OnRawData(nullptr);
            result.errorMessage = "Variable payload is empty.";
            return result;
        }

        std::vector<uint8_t> bytes;
        std::string validationError;
        if (!ValueValidator::Encode(var.type, payload, bytes, validationError)) {
            streamSession->OnRawData(nullptr);
            result.errorMessage = "Invalid value for " + var.type + ": " + validationError;
            return result;
        }

        const bool sendSuccess = streamSession->SendRaw(bytes.data(), bytes.size());
        if (!sendSuccess) {
            streamSession->OnRawData(nullptr);
            result.errorMessage = "Failed to send payload";
            return result;
        }
    }

    {
        std::unique_lock<std::mutex> lock(state->mtx);
        state->cv.wait_for(lock, std::chrono::milliseconds(m_waitMs),
                           [&]() { return state->dataReceived; });
    }

    streamSession->OnRawData(nullptr);

    if (state->dataReceived) {
        var.currentValue = state->receivedData;
        result.rawResponse = state->receivedData;
    } else {
        result.rawResponse = var.currentValue;
    }

    result.success = true;
    return result;
}

bool TcpSendStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    if (m_assertType.empty()) return true;

    if (m_assertType == "regex") {
        try {
            std::regex re(m_expectedValue);
            if (std::regex_search(result.rawResponse, re)) return true;
            outErrorMsg = "Response does not match regex.";
            return false;
        } catch (const std::regex_error&) {
            outErrorMsg = "Invalid regex pattern.";
            return false;
        }
    }

    if (m_assertType == "==") {
        if (result.rawResponse == m_expectedValue) return true;
        outErrorMsg = "Expected '" + m_expectedValue + "', got '" + result.rawResponse + "'";
        return false;
    }

    if (m_assertType == "!=") {
        if (result.rawResponse != m_expectedValue) return true;
        outErrorMsg = "Expected not equal to '" + m_expectedValue + "'";
        return false;
    }

    if (m_assertType == ">" || m_assertType == "<") {
        try {
            double actual = std::stod(result.rawResponse);
            double expected = std::stod(m_expectedValue);
            if (m_assertType == ">" && actual > expected) return true;
            if (m_assertType == "<" && actual < expected) return true;
            outErrorMsg =
                "Expected " + m_assertType + " " + m_expectedValue + ", got " + result.rawResponse;
            return false;
        } catch (...) {
            outErrorMsg = "Cannot compare non-numeric values.";
            return false;
        }
    }
    return true;
}

std::vector<std::string> TcpSendStep::Extract(const TestResult& result,
                                              std::map<std::string, CommVariable>& vars) {
    std::vector<std::string> updated;
    if (!result.rawResponse.empty() && vars.find(m_variableName) != vars.end()) {
        vars[m_variableName].currentValue = result.rawResponse;
        updated.push_back(m_variableName);
    }
    return updated;
}

}  // namespace icu
