#pragma once

#include <wx/string.h>
#include <wx/translation.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace icu {

// 通信变量值的类型校验与编码。
//   - UI 在发送/写入前用它拦截非法输入；
//   - 测试步骤在 Execute 前用它避免把非法内容发到设备。
class ValueValidator {
public:
    static bool IsIntegerType(const std::string& type) {
        return type == "int8" || type == "uint8" || type == "int16" || type == "uint16" ||
               type == "int" || type == "int32" || type == "uint" || type == "uint32";
    }

    static bool IsFloatType(const std::string& type) { return type == "float" || type == "double"; }

    static std::string Trim(const std::string& s) {
        const auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return {};
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    // 仅校验，不产生字节。
    static bool Validate(const std::string& type, const std::string& rawText, std::string& error) {
        error.clear();
        const std::string text = Trim(rawText);

        if (type == "string" || type.empty()) {
            return true;  // 字符串可以是任意内容（包括空串）
        }

        if (type == "hex") {
            std::vector<uint8_t> bytes;
            return ParseHexBytes(text, bytes, error);
        }

        if (type == "bool") {
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == "true" || lower == "false" || lower == "1" || lower == "0" ||
                lower == "yes" || lower == "no" || lower == "on" || lower == "off") {
                return true;
            }
            error = Tr(wxTRANSLATE("Expected a boolean value (true/false/1/0)."));
            return false;
        }

        if (IsIntegerType(type)) {
            long long v = 0;
            if (!ParseInteger(text, v, error)) return false;

            long long lo = std::numeric_limits<long long>::min();
            long long hi = std::numeric_limits<long long>::max();
            if (type == "int8") {
                lo = -128;
                hi = 127;
            } else if (type == "uint8") {
                lo = 0;
                hi = 255;
            } else if (type == "int16") {
                lo = -32768;
                hi = 32767;
            } else if (type == "uint16") {
                lo = 0;
                hi = 65535;
            } else if (type == "int" || type == "int32") {
                lo = std::numeric_limits<int32_t>::min();
                hi = std::numeric_limits<int32_t>::max();
            } else if (type == "uint" || type == "uint32") {
                lo = 0;
                hi = static_cast<long long>(std::numeric_limits<uint32_t>::max());
            }
            if (v < lo || v > hi) {
                error = Tr(wxTRANSLATE("Value is out of range for the selected type."));
                return false;
            }
            return true;
        }

        if (IsFloatType(type)) {
            double v = 0.0;
            if (!ParseDouble(text, v, error)) return false;
            if (!std::isfinite(v)) {
                error = Tr(wxTRANSLATE("Expected a finite number."));
                return false;
            }
            return true;
        }

        // 未知类型按字符串处理，避免误拦截自定义类型
        return true;
    }

    // 校验并编码为网络字节序（大端）。
    static bool Encode(const std::string& type, const std::string& rawText,
                       std::vector<uint8_t>& bytes, std::string& error) {
        bytes.clear();
        if (!Validate(type, rawText, error)) return false;

        const std::string text = Trim(rawText);

        if (type == "bool") {
            std::string lower = text;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool on = (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
            bytes.push_back(on ? 1 : 0);
        } else if (type == "int8") {
            bytes.push_back(static_cast<uint8_t>(static_cast<int8_t>(std::stoll(text))));
        } else if (type == "uint8") {
            bytes.push_back(static_cast<uint8_t>(std::stoull(text)));
        } else if (type == "int16" || type == "uint16") {
            const uint16_t v = static_cast<uint16_t>(std::stoll(text));
            bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        } else if (type == "int" || type == "int32" || type == "uint" || type == "uint32") {
            const uint32_t v = static_cast<uint32_t>(std::stoll(text));
            bytes.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(v & 0xFF));
        } else if (type == "float") {
            const float v = std::stof(text);
            uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            bytes.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(bits & 0xFF));
        } else if (type == "double") {
            const double v = std::stod(text);
            uint64_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            for (int i = 7; i >= 0; --i) {
                bytes.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
            }
        } else if (type == "hex") {
            if (!ParseHexBytes(text, bytes, error)) return false;
        } else {
            bytes.assign(text.begin(), text.end());
        }
        return true;
    }

    // 十六进制文本 -> 字节。支持 "01 0A FF"、"0x01,0x0A"、"010AFF" 等写法。
    static bool ParseHexBytes(const std::string& rawText, std::vector<uint8_t>& bytes,
                              std::string& error) {
        bytes.clear();
        const std::string text = Trim(rawText);
        if (text.empty()) {
            error = Tr(wxTRANSLATE("Hex payload is empty."));
            return false;
        }

        std::string digits;
        for (size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            // 跳过 0x / 0X 前缀（前缀里的 0 不是数据）
            if (c == '0' && i + 1 < text.size() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
                ++i;
                continue;
            }
            if (std::isxdigit(static_cast<unsigned char>(c))) {
                digits.push_back(c);
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ';' ||
                       c == '-' || c == ':') {
                continue;
            } else {
                error = Tr(wxTRANSLATE("Invalid hex character."));
                return false;
            }
        }

        if (digits.empty() || digits.size() % 2 != 0) {
            error = Tr(wxTRANSLATE("Hex payload must contain an even number of hex digits."));
            return false;
        }
        bytes.reserve(digits.size() / 2);
        for (size_t i = 0; i < digits.size(); i += 2) {
            bytes.push_back(static_cast<uint8_t>(std::stoul(digits.substr(i, 2), nullptr, 16)));
        }
        return true;
    }

private:
    static std::string Tr(const char* text) {
        return std::string(wxGetTranslation(wxString::FromUTF8(text)).utf8_str());
    }

    static bool ParseInteger(const std::string& text, long long& out, std::string& error) {
        if (text.empty()) {
            error = Tr(wxTRANSLATE("Value is empty."));
            return false;
        }
        size_t pos = 0;
        try {
            out = std::stoll(text, &pos, 10);
        } catch (...) {
            error = Tr(wxTRANSLATE("Expected an integer."));
            return false;
        }
        if (pos != text.size()) {
            error = Tr(wxTRANSLATE("Expected an integer."));
            return false;
        }
        return true;
    }

    static bool ParseDouble(const std::string& text, double& out, std::string& error) {
        if (text.empty()) {
            error = Tr(wxTRANSLATE("Value is empty."));
            return false;
        }
        size_t pos = 0;
        try {
            out = std::stod(text, &pos);
        } catch (...) {
            error = Tr(wxTRANSLATE("Expected a number."));
            return false;
        }
        if (pos != text.size()) {
            error = Tr(wxTRANSLATE("Expected a number."));
            return false;
        }
        return true;
    }
};

}  // namespace icu
