#pragma once

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/wx.h>

namespace icu {

class LogFrame : public wxDialog {
public:
    explicit LogFrame(wxWindow* parent);
    ~LogFrame() override;

    void ForceDestroy() { Destroy(); }

private:
    wxTextCtrl* m_textCtrl = nullptr;
    wxCheckBox* m_autoScroll = nullptr;
    wxButton* m_clearBtn = nullptr;
    wxButton* m_loadBtn = nullptr;

    void BuildUi();

    void OnClose(wxCloseEvent& evt);
    void OnClear(wxCommandEvent& evt);
    void OnLoad(wxCommandEvent& evt);

    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
