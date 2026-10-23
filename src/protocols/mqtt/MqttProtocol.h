#pragma once

#include <wx/translation.h>

#include <nlohmann/json.hpp>

#include "core/IProtocol.h"

namespace icu {

class MqttProtocol : public IProtocol {
public:
    std::string Id() const override { return "mqtt"; }
    std::string DisplayName() const override { return _("&MQTT").utf8_string(); }

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
    std::vector<FieldDescriptor> ClientFields() const;
    std::vector<FieldDescriptor> SubscriberFields() const;
    std::vector<FieldDescriptor> BrokerFields() const;
};

}  // namespace icu
