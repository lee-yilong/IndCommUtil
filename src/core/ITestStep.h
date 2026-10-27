#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "SessionConfig.h"

namespace icu {

class ISession;

struct TestResult {
    bool success = false;
    bool isSendAction = false;  // true 表示这是写入/发送步骤
    std::string errorMessage;
    std::string rawResponse;          // 解析后的响应/写入值，用于断言与日志
    std::string rawBytesHex;          // 原始字节的十六进制表示（用于变量列表显示）
    std::vector<uint16_t> registers;  // 用于 Modbus 数值比对
};

class ITestStep {
public:
    virtual ~ITestStep() = default;

    virtual std::string Name() const = 0;

    // 执行测试动作（从 vars 中读取 params，执行并返回结果）
    virtual TestResult Execute(ISession* session, std::map<std::string, CommVariable>& vars) = 0;

    // 断言校验
    virtual bool Assert(const TestResult& result, std::string& outErrorMsg) = 0;

    // 将执行结果写入变量池（由 TestRunner 调用）
    // ★ 返回值：返回被更新过的变量名列表
    virtual std::vector<std::string> Extract(const TestResult& result,
                                             std::map<std::string, CommVariable>& vars) = 0;

    // 执行成功后的固定等待（毫秒）。已经自行等待过响应的步骤返回 0，避免重复等待。
    virtual int WaitMs() const { return 0; }
};

using ITestStepPtr = std::unique_ptr<ITestStep>;

}  // namespace icu
