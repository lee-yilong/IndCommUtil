#pragma once

#include <wx/translation.h>

#include "core/IProtocol.h"

namespace icu {

// 串口协议：单模式（点对点，无 Client/Server 角色区分）
// HasModes() = false，ModeNames() 返回空
class SerialProtocol : public IProtocol {
public:
    std::string Id() const override { return "serial"; }
    std::string DisplayName() const override { return _("&Serial Port").utf8_string(); }

    bool HasModes() const override { return false; }
    std::vector<std::string> ModeNames() const override { return {}; }

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;

    SessionPtr CreateSession(int modeIndex,
                             const std::map<std::string, FieldValue>& fields) override;

    std::string MakeDefaultFileName(int modeIndex,
                                    const std::map<std::string, FieldValue>& fields) const override;
};

}  // namespace icu
