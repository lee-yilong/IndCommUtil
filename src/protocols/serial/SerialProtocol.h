#pragma once

#include <wx/translation.h>

#include "core/IProtocol.h"

namespace icu {

class SerialProtocol : public IProtocol {
public:
    std::string Id() const override { return "serial"; }
    std::string DisplayName() const override { return _("&Serial Port").utf8_string(); }

    bool HasModes() const override { return false; }
    std::vector<std::string> ModeNames() const override { return {}; }

    std::vector<FieldDescriptor> FieldsForMode(int modeIndex) const override;

    ISessionPtr CreateSession(int modeIndex,
                              const std::map<std::string, FieldValue>& fields) override;

    std::string MakeDefaultFileName(int modeIndex,
                                    const std::map<std::string, FieldValue>& fields) const override;
};

}  // namespace icu
