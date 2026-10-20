#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace icu {

class BytesParser {
public:
    static bool Parse(const std::string& type, const std::vector<uint8_t>& data,
                      std::string& outValue) {
        if (data.empty()) return false;

        if (type == "bool") {
            outValue = (data[0] != 0) ? "true" : "false";
            return true;
        }
        if (type == "int8") {
            if (data.size() < 1) return false;
            outValue = std::to_string(static_cast<int8_t>(data[0]));
            return true;
        }
        if (type == "uint8") {
            if (data.size() < 1) return false;
            outValue = std::to_string(data[0]);
            return true;
        }
        if (type == "int16") {
            if (data.size() < 2) return false;
            int16_t val = (data[0] << 8) | data[1];
            outValue = std::to_string(val);
            return true;
        }
        if (type == "uint16") {
            if (data.size() < 2) return false;
            uint16_t val = (data[0] << 8) | data[1];
            outValue = std::to_string(val);
            return true;
        }
        // ★ 兼容旧的 int / uint 类型
        if (type == "int" || type == "int32") {
            if (data.size() < 4) return false;
            int32_t val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            outValue = std::to_string(val);
            return true;
        }
        if (type == "uint" || type == "uint32") {
            if (data.size() < 4) return false;
            uint32_t val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            outValue = std::to_string(val);
            return true;
        }
        if (type == "float") {
            if (data.size() < 4) return false;
            uint32_t intVal = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            float val;
            std::memcpy(&val, &intVal, sizeof(float));
            std::ostringstream oss;
            oss << val;
            outValue = oss.str();
            return true;
        }
        if (type == "double") {
            if (data.size() < 8) return false;
            uint64_t intVal = 0;
            for (int i = 0; i < 8; ++i) intVal = (intVal << 8) | data[i];
            double val;
            std::memcpy(&val, &intVal, sizeof(double));
            std::ostringstream oss;
            oss << val;
            outValue = oss.str();
            return true;
        }
        if (type == "hex") {
            std::ostringstream oss;
            for (uint8_t b : data) {
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            outValue = oss.str();
            return true;
        }

        // 默认作为 ASCII / UTF-8 字符串处理 (string)
        outValue = std::string(data.begin(), data.end());
        return true;
    }
};

}  // namespace icu
