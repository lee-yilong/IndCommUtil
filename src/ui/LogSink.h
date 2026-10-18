#pragma once

#include <wx/string.h>
#include <wx/textctrl.h>

namespace icu::log {

void AttachGuiSink(wxTextCtrl* ctrl);
void DetachGuiSink();

}  // namespace icu::log
