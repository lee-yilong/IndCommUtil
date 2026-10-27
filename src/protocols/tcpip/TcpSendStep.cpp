#include "TcpSendStep.h"

#include <wx/string.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>

#include "core/BytesParser.h"
#include "core/StreamSession.h"
#include "core/ValueValidator.h"
#include "log/Log.h"

namespace icu {

namespace {

std::string Trim(const std::string& value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    auto begin = std::find_if(value.begin(), value.end(), notSpace);
    auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    if (begin >= end) return {};
    return std::string(begin, end);
}

std::string HexBytes(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) oss << ' ';
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

bool ParseNumber(const std::string& text, double& out) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) return false;
    errno = 0;
    char* end = nullptr;
    out = std::strtod(trimmed.c_str(), &end);
    return errno == 0 && end != trimmed.c_str() && *end == '\0' && std::isfinite(out);
}

std::string JsonValueToString(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_number()) {
        std::ostringstream oss;
        oss << value.get<double>();
        return oss.str();
    }
    return value.dump();
}

}  // namespace

TcpSendStep::TcpSendStep(const nlohmann::json& params) {
    m_name = params.value("name", "TCP Step");
    m_variableName = params.value("variable", "");
    m_waitMs = params.value("wait_ms", 1000);

    if (params.contains("value")) m_writeValue = JsonValueToString(params["value"]);

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

    if (!m_isReadOnly) {
        // 写入值优先使用步骤里配置的目标值；没有配置时才回退到变量当前值
        const std::string payload = m_writeValue.empty() ? var.currentValue : m_writeValue;
        if (payload.empty()) {
            result.errorMessage = "Variable payload is empty.";
            return result;
        }

        std::vector<uint8_t> sentBytes;
        std::string validationError;
        if (!ValueValidator::Encode(var.type, payload, sentBytes, validationError)) {
            result.errorMessage = "Invalid value for " + var.type + ": " + validationError;
            return result;
        }

        if (!streamSession->SendRaw(sentBytes.data(), sentBytes.size())) {
            result.errorMessage = "Failed to send payload";
            return result;
        }

        // 写步骤不在这里等待响应：等待交给 TestRunner 的 WaitMs，
        // 避免把后续读步骤需要的响应提前消费掉。
        result.isSendAction = true;
        result.rawResponse = payload;
        result.rawBytesHex = HexBytes(sentBytes);
        result.success = true;
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

    {
        std::unique_lock<std::mutex> lock(state->mtx);
        state->cv.wait_for(lock, std::chrono::milliseconds(std::max(m_waitMs, 0)),
                           [&]() { return state->dataReceived; });
    }

    std::string receivedData;
    {
        std::lock_guard<std::mutex> lock(state->mtx);
        receivedData = state->receivedData;
    }
    streamSession->OnRawData(nullptr);

    if (receivedData.empty()) {
        result.errorMessage =
            wxString::Format(_("No response received within %d ms."), m_waitMs).utf8_string();
        return result;
    }

    const std::vector<uint8_t> receivedBytes(receivedData.begin(), receivedData.end());
    std::string decoded;
    if (!BytesParser::Parse(var.type, receivedBytes, decoded)) {
        decoded = receivedData;
    }
    if (var.type == "string") {
        while (!decoded.empty() &&
               (decoded.back() == '\0' || decoded.back() == '\r' || decoded.back() == '\n')) {
            decoded.pop_back();
        }
    }

    result.rawResponse = decoded;
    result.rawBytesHex = HexBytes(receivedBytes);
    result.success = true;
    return result;
}

bool TcpSendStep::Assert(const TestResult& result, std::string& outErrorMsg) {
    if (m_assertType.empty()) return true;

    const std::string actual = Trim(result.rawResponse);
    const std::string expected = Trim(m_expectedValue);

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

    double actualNum = 0.0;
    double expectedNum = 0.0;
    const bool bothNumeric = ParseNumber(actual, actualNum) && ParseNumber(expected, expectedNum);

    if (m_assertType == "==") {
        if (bothNumeric ? std::fabs(actualNum - expectedNum) < 1e-9 : actual == expected)
            return true;
        outErrorMsg = "Expected '" + expected + "', got '" + actual + "'";
        return false;
    }

    if (m_assertType == "!=") {
        if (bothNumeric ? std::fabs(actualNum - expectedNum) >= 1e-9 : actual != expected)
            return true;
        outErrorMsg = "Expected not equal to '" + expected + "'";
        return false;
    }

    if (m_assertType == ">" || m_assertType == "<") {
        if (!bothNumeric) {
            outErrorMsg = "Cannot compare non-numeric values.";
            return false;
        }
        if (m_assertType == ">" && actualNum > expectedNum) return true;
        if (m_assertType == "<" && actualNum < expectedNum) return true;
        outErrorMsg = "Expected " + m_assertType + " " + expected + ", got " + actual;
        return false;
    }
    return true;
}

std::vector<std::string> TcpSendStep::Extract(const TestResult& result,
                                              std::map<std::string, CommVariable>& vars) {
    std::vector<std::string> updated;
    if (!result.rawResponse.empty() && vars.find(m_variableName) != vars.end()) {
        auto& var = vars[m_variableName];
        var.currentValue = result.rawResponse;
        if (!result.rawBytesHex.empty()) var.rawBytes = result.rawBytesHex;
        updated.push_back(m_variableName);
    }
    return updated;
}

}  // namespace icu
