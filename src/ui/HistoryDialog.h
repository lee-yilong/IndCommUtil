#pragma once

#include <wx/checklst.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
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
    void OnRangeChanged(wxCommandEvent& evt);

    Database* m_db;
    std::string m_projectPath;

    wxCheckListBox* m_varList;
    wxSpinCtrl* m_rangeValue;
    wxChoice* m_rangeUnit;
    wxDatePickerCtrl* m_startDate;
    wxTimePickerCtrl* m_startTime;
    wxDatePickerCtrl* m_endDate;
    wxTimePickerCtrl* m_endTime;
    wxNotebook* m_resultBook = nullptr;
    ChartPanel* m_chart = nullptr;
    wxListCtrl* m_textList = nullptr;

    wxPoint m_dragStart;
    wxPoint m_winStart;
};

}  // namespace icu
