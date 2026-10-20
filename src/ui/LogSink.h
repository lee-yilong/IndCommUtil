#pragma once

#include <wx/string.h>
#include <wx/textctrl.h>

#include <vector>

namespace icu::log {

void AttachGuiSink(wxTextCtrl* ctrl);
void DetachGuiSink();

std::vector<wxString> GetLogHistory();
void ClearLogHistory();

}  // namespace icu::log
