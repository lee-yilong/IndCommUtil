#pragma once

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

namespace icu {

class LogFrame : public wxFrame {
public:
    explicit LogFrame(wxWindow* parent);
    ~LogFrame() override;

    void ForceDestroy();

private:
    wxTextCtrl* m_textCtrl = nullptr;
    wxCheckBox* m_autoScroll = nullptr;
    wxButton* m_clearBtn = nullptr;
    wxButton* m_loadBtn = nullptr;  // ★ 新增：加载日志按钮

    bool m_forceDestroy = false;

    void BuildUi();

    void OnClose(wxCloseEvent& evt);
    void OnClear(wxCommandEvent& evt);
    void OnLoad(wxCommandEvent& evt);  // ★ 新增：处理加载逻辑

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
