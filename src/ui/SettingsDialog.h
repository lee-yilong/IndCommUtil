#pragma once

#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

#include <functional>

namespace icu {

class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent);
    ~SettingsDialog() override = default;

    using ApplyCallback = std::function<void()>;
    void SetApplyCallback(ApplyCallback cb) { m_onApply = std::move(cb); }

private:
    void BuildUi();
    void LoadSettings();
    void SaveSettings();

    wxNotebook* m_notebook = nullptr;

    wxCheckBox* m_minimizeToTray = nullptr;
    wxSpinCtrl* m_fontSizeCtrl = nullptr;
    wxCheckBox* m_hexDisplay = nullptr;
    wxCheckBox* m_autoScroll = nullptr;
    wxCheckBox* m_showTimestamp = nullptr;

    ApplyCallback m_onApply;

    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
