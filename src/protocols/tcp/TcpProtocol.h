#pragma once

#include "core/IProtocol.h"

namespace icu {

// TCP 协议：一个类，包含 Client / Server 两种模式
// 模式索引：0 = Client, 1 = Server
class TcpProtocol : public IProtocol {
public:
    std::string Id() const override { return "tcp"; }
    std::string DisplayName() const override { return "&TCP/IP"; }

    bool HasModes() const override { return true; }
    std::vector<std::string> ModeNames() const override;

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;

    SessionPtr CreateSession(int modeIndex,
                             const std::map<std::string, FieldValue>& fields) override;

    std::string MakeDefaultFileName(int modeIndex,
                                    const std::map<std::string, FieldValue>& fields) const override;

private:
    std::vector<FieldDescriptor> ClientFields() const;
    std::vector<FieldDescriptor> ServerFields() const;
};

}  // namespace icu
