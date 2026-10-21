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

    // ★ 应用设置后的回调
    using ApplyCallback = std::function<void()>;
    void SetApplyCallback(ApplyCallback cb) { m_onApply = std::move(cb); }

private:
    void BuildUi();
    void LoadSettings();
    void SaveSettings();

    void OnApply(wxCommandEvent& evt);
    void OnOk(wxCommandEvent& evt);
    void OnCancel(wxCommandEvent& evt);

    wxNotebook* m_notebook = nullptr;

    // 通用
    wxCheckBox* m_minimizeToTray = nullptr;

    // 界面与日志
    wxSpinCtrl* m_fontSizeCtrl = nullptr;
    wxCheckBox* m_hexDisplay = nullptr;
    wxCheckBox* m_autoScroll = nullptr;
    wxCheckBox* m_showTimestamp = nullptr;

    ApplyCallback m_onApply;
};

}  // namespace icu
