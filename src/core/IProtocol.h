#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "ISession.h"
#include "ITestStep.h"
#include "ProtocolTypes.h"

namespace icu {

class IProtocol {
public:
    virtual ~IProtocol() = default;

    virtual std::string Id() const = 0;
    virtual std::string DisplayName() const = 0;

    virtual bool HasModes() const = 0;
    virtual std::vector<std::string> ModeNames() const = 0;

    virtual std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const = 0;

    // 默认返回 nullptr，由 StreamProtocol 实现具体的字节流 Session
    virtual ISessionPtr CreateSession(int modeIndex,
                                      const std::map<std::string, FieldValue>& fields) {
        return nullptr;
    }

    virtual std::string MakeDefaultFileName(
        int modeIndex, const std::map<std::string, FieldValue>& fields) const = 0;

    // 返回事务型协议的操作区字段；字节流协议返回空
    virtual std::vector<FieldDescriptor> OperationFieldsForMode(int /*modeIndex*/) const {
        return {};
    }

    virtual ITestStepPtr CreateTestStep(const std::string& stepType, const nlohmann::json& params) {
        return nullptr;  // 默认不支持测试步骤
    }
};

using IProtocolPtr = std::unique_ptr<IProtocol>;

}  // namespace icu
