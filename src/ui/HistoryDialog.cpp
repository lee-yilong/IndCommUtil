#include "HistoryDialog.h"

#include <wx/datetime.h>
#include <wx/statbox.h>
#include <wx/statline.h>

#include <algorithm>
#include <chrono>

namespace icu {

HistoryDialog::HistoryDialog(wxWindow* parent, Database* db, const std::string& projectPath,
                             const std::vector<std::string>& availableVars)
    : wxDialog(parent, wxID_ANY, _("History Query"), wxDefaultPosition, wxSize(900, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_db(db),
      m_projectPath(projectPath) {
    auto* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    // --- 左侧：查询条件 ---
    auto* leftPanel = new wxPanel(this);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    auto* varBox = new wxStaticBoxSizer(wxVERTICAL, leftPanel, _("Select Variables"));
    m_varList = new wxCheckListBox(varBox->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, availableVars);
    for (unsigned int i = 0; i < m_varList->GetCount(); ++i) m_varList->Check(i, true);
    varBox->Add(m_varList, 1, wxEXPAND | wxALL, 5);
    leftSizer->Add(varBox, 1, wxEXPAND | wxALL, 5);

    auto* timeBox = new wxStaticBoxSizer(wxVERTICAL, leftPanel, _("Time Range"));
    auto* startSizer = new wxBoxSizer(wxHORIZONTAL);
    startSizer->Add(new wxStaticText(timeBox->GetStaticBox(), wxID_ANY, _("Start: ")), 0,
                    wxALIGN_CENTER_VERTICAL);

    auto now = wxDateTime::Now();
    auto yesterday = now - wxDateSpan::Day();

    m_startDate = new wxDatePickerCtrl(timeBox->GetStaticBox(), wxID_ANY, yesterday);
    m_startTime = new wxTimePickerCtrl(timeBox->GetStaticBox(), wxID_ANY, yesterday);
    startSizer->Add(m_startDate, 1, wxEXPAND | wxRIGHT, 5);
    startSizer->Add(m_startTime, 1, wxEXPAND);
    timeBox->Add(startSizer, 0, wxEXPAND | wxALL, 5);

    auto* endSizer = new wxBoxSizer(wxHORIZONTAL);
    endSizer->Add(new wxStaticText(timeBox->GetStaticBox(), wxID_ANY, _("End:   ")), 0,
                  wxALIGN_CENTER_VERTICAL);
    m_endDate = new wxDatePickerCtrl(timeBox->GetStaticBox(), wxID_ANY, now);
    m_endTime = new wxTimePickerCtrl(timeBox->GetStaticBox(), wxID_ANY, now);
    endSizer->Add(m_endDate, 1, wxEXPAND | wxRIGHT, 5);
    endSizer->Add(m_endTime, 1, wxEXPAND);
    timeBox->Add(endSizer, 0, wxEXPAND | wxALL, 5);

    leftSizer->Add(timeBox, 0, wxEXPAND | wxALL, 5);

    auto* btnQuery = new wxButton(leftPanel, wxID_ANY, _("Query"));
    btnQuery->Bind(wxEVT_BUTTON, &HistoryDialog::OnQuery, this);
    leftSizer->Add(btnQuery, 0, wxEXPAND | wxALL, 10);

    leftPanel->SetSizer(leftSizer);
    mainSizer->Add(leftPanel, 0, wxEXPAND | wxALL, 5);

    // --- 右侧：图表 ---
    m_chart = new ChartPanel(this);
    mainSizer->Add(m_chart, 1, wxEXPAND | wxALL, 5);

    SetSizer(mainSizer);
    CentreOnParent();
}

void HistoryDialog::OnQuery(wxCommandEvent&) {
    std::vector<std::string> selectedVars;
    for (unsigned int i = 0; i < m_varList->GetCount(); ++i) {
        if (m_varList->IsChecked(i)) {
            selectedVars.push_back(std::string(m_varList->GetString(i).utf8_str()));
        }
    }

    if (selectedVars.empty()) {
        wxMessageBox(_("Please select at least one variable."), _("Notice"),
                     wxOK | wxICON_INFORMATION, this);
        return;
    }

    // 正确提取时间
    wxDateTime start = m_startDate->GetValue();
    wxDateTime st = m_startTime->GetValue();
    start.SetHour(st.GetHour());
    start.SetMinute(st.GetMinute());
    start.SetSecond(st.GetSecond());
    start.SetMillisecond(0);

    wxDateTime end = m_endDate->GetValue();
    wxDateTime et = m_endTime->GetValue();
    end.SetHour(et.GetHour());
    end.SetMinute(et.GetMinute());
    end.SetSecond(et.GetSecond());
    end.SetMillisecond(999);

    // wxDateTime::GetValue() 返回的是从 1970-01-01 到现在的毫秒数
    long long startMs = start.GetValue().GetValue();
    long long endMs = end.GetValue().GetValue();

    double startSec = static_cast<double>(startMs) / 1000.0;
    double endSec = static_cast<double>(endMs) / 1000.0;

    auto records = m_db->QueryHistory(m_projectPath, startSec, endSec);

    m_chart->Clear();
    m_chart->SetTimeWindow(endSec - startSec);  // 设置时间窗口为查询跨度

    int loadedCount = 0;
    for (const auto& rec : records) {
        // 过滤未勾选的变量
        if (std::find(selectedVars.begin(), selectedVars.end(), rec.varName) ==
            selectedVars.end()) {
            continue;
        }
        m_chart->AddDataPointWithTime(rec.varName, rec.value, rec.timeSec);
        loadedCount++;
    }

    if (loadedCount == 0) {
        wxMessageBox(_("No data found in this time range."), _("Notice"), wxOK | wxICON_INFORMATION,
                     this);
    }
}

}  // namespace icu
