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
    std::string errorMessage;
    std::string rawResponse;          // 用于 TCP 正则匹配
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
};

using ITestStepPtr = std::unique_ptr<ITestStep>;

}  // namespace icu
