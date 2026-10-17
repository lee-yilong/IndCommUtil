#pragma once

#include <wx/translation.h>

#include "core/IProtocol.h"

namespace icu {

class ModbusProtocol : public IProtocol {
public:
    std::string Id() const override { return "modbus"; }
    std::string DisplayName() const override { return _("&Modbus").utf8_string(); }

    bool HasModes() const override { return true; }
    std::vector<std::string> ModeNames() const override;

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;

    // 这就是刚刚加上去的自动生成表单字段
    std::vector<FieldDescriptor> OperationFieldsForMode(int modeIndex) const override;

    ISessionPtr CreateSession(int modeIndex,
                              const std::map<std::string, FieldValue>& fields) override;

    std::string MakeDefaultFileName(int modeIndex,
                                    const std::map<std::string, FieldValue>& fields) const override;

private:
    std::vector<FieldDescriptor> TcpFields() const;
    std::vector<FieldDescriptor> RtuFields() const;
    std::vector<FieldDescriptor> AsciiFields() const;
};

}  // namespace icu
