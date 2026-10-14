#include "IconSettings.h"

#include <wx/artprov.h>

namespace icu::settings {

IconSettings& IconSettings::Instance() {
    static IconSettings inst;
    return inst;
}

wxBitmap IconSettings::GetProtocolIcon(const std::string&) const {
    return wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_OTHER, wxSize(16, 16));
}

wxBitmap IconSettings::GetConnectIcon() const {
    return wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_OTHER, wxSize(16, 16));
}

wxBitmap IconSettings::GetDisconnectIcon() const {
    return wxArtProvider::GetBitmap(wxART_STOP, wxART_OTHER, wxSize(16, 16));
}

wxBitmap IconSettings::GetSendIcon() const {
    return wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_OTHER, wxSize(16, 16));
}

wxBitmap IconSettings::GetClearIcon() const {
    return wxArtProvider::GetBitmap(wxART_DELETE, wxART_OTHER, wxSize(16, 16));
}

}  // namespace icu::settings
