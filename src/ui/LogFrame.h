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

    bool m_forceDestroy = false;

    void BuildUi();

    void OnClose(wxCloseEvent& evt);
    void OnClear(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
