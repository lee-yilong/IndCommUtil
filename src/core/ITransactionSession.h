#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/ISession.h"
#include "core/ProtocolTypes.h"

namespace icu {

struct TransactionRequest {
    int slaveId = 1;
    int functionCode = 0;
    int startAddress = 0;
    int count = 0;
    std::vector<uint16_t> writeValues;
};

struct TransactionResponse {
    bool ok = false;
    std::string errorMessage;
    std::vector<uint16_t> registers;
    std::vector<bool> coils;
};

class ITransactionSession : public ISession {
public:
    ~ITransactionSession() override = default;

    virtual TransactionResponse Execute(const TransactionRequest& request) = 0;
    virtual TransactionResponse Execute(const std::map<std::string, FieldValue>& fields) = 0;

    // 新增：供底层向 UI 推送状态日志
    using LogCallback = std::function<void(const std::string&)>;
    virtual void OnLog(LogCallback cb) = 0;
};

using ITransactionSessionPtr = std::unique_ptr<ITransactionSession>;

}  // namespace icu
