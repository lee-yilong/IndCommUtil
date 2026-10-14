#pragma once

#include <wx/bitmap.h>

#include <string>

namespace icu::settings {

class IconSettings {
public:
    static IconSettings& Instance();

    wxBitmap GetProtocolIcon(const std::string& protocolId) const;
    wxBitmap GetConnectIcon() const;
    wxBitmap GetDisconnectIcon() const;
    wxBitmap GetSendIcon() const;
    wxBitmap GetClearIcon() const;

private:
    IconSettings() = default;
};

}  // namespace icu::settings
