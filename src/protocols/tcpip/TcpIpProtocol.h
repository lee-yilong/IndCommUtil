#pragma once

#include <wx/translation.h>

#include <nlohmann/json.hpp>

#include "core/IProtocol.h"

namespace icu {

class TcpIpProtocol : public IProtocol {
public:
    std::string Id() const override { return "tcp"; }
    std::string DisplayName() const override { return _("&TCP/IP").utf8_string(); }

    bool HasModes() const override { return true; }
    std::vector<std::string> ModeNames() const override;

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;

    ISessionPtr CreateSession(int modeIndex,
                              const std::map<std::string, FieldValue>& fields) override;

    std::string MakeDefaultFileName(int modeIndex,
                                    const std::map<std::string, FieldValue>& fields) const override;

    ITestStepPtr CreateTestStep(const std::string& stepType, const nlohmann::json& params) override;

    std::vector<FieldDescriptor> VariableParamsFields() const override;

private:
    std::vector<FieldDescriptor> TcpFields() const;
    std::vector<FieldDescriptor> UdpFields() const;
};

}  // namespace icu
