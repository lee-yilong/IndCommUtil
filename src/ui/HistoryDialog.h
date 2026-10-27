#pragma once

#include <wx/checklst.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/timectrl.h>
#include <wx/wx.h>

#include "core/Database.h"
#include "ui/ChartPanel.h"

namespace icu {

class HistoryDialog : public wxDialog {
public:
    HistoryDialog(wxWindow* parent, Database* db, const std::string& projectPath,
                  const std::vector<std::string>& availableVars);
    ~HistoryDialog() override = default;

private:
    void OnQuery(wxCommandEvent& evt);

    Database* m_db;
    std::string m_projectPath;

    wxCheckListBox* m_varList;
    wxDatePickerCtrl* m_startDate;
    wxTimePickerCtrl* m_startTime;
    wxDatePickerCtrl* m_endDate;
    wxTimePickerCtrl* m_endTime;
    ChartPanel* m_chart;

    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
