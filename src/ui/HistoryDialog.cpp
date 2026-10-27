#include "HistoryDialog.h"

#include <wx/datetime.h>
#include <wx/statbox.h>
#include <wx/statline.h>

#include <algorithm>
#include <chrono>

#include "ui/UIUtils.h"

namespace icu {

namespace {
const int kTimeUnitMultipliers[] = {1, 60, 3600, 86400};
constexpr int kTimeUnitCount = 4;
}  // namespace

HistoryDialog::HistoryDialog(wxWindow* parent, Database* db, const std::string& projectPath,
                             const std::vector<std::string>& availableVars)
    : wxDialog(parent, wxID_ANY, _("History Query"), wxDefaultPosition, wxSize(900, 600),
               wxBORDER_NONE | wxRESIZE_BORDER),
      m_db(db),
      m_projectPath(projectPath) {
    SetBackgroundColour(ui::kBgColor);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. 标题栏
    auto* header = ui::CreateDialogHeader(this, _("History Query"), m_dragStart, m_winStart,
                                          [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // --- 左侧：查询条件卡片 ---
    auto* leftCard = new ui::CardPanel(this);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    auto* varTitle = new wxStaticText(leftCard, wxID_ANY, _("Select Variables"));
    varTitle->SetFont(varTitle->GetFont().Bold());
    varTitle->SetForegroundColour(ui::kThemeDark);
    leftSizer->Add(varTitle, 0, wxALL, 12);

    m_varList =
        new wxCheckListBox(leftCard, wxID_ANY, wxDefaultPosition, wxDefaultSize, availableVars);
    for (unsigned int i = 0; i < m_varList->GetCount(); ++i) m_varList->Check(i, true);
    leftSizer->Add(m_varList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* quickTitle = new wxStaticText(leftCard, wxID_ANY, _("Quick Range"));
    quickTitle->SetFont(quickTitle->GetFont().Bold());
    quickTitle->SetForegroundColour(ui::kThemeDark);
    leftSizer->Add(quickTitle, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    m_rangeValue = new wxSpinCtrl(leftCard, wxID_ANY, "24", wxDefaultPosition, wxSize(-1, -1),
                                  wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 1, 100000, 24);
    m_rangeUnit = new wxChoice(leftCard, wxID_ANY);
    m_rangeUnit->Append(_("Seconds"));
    m_rangeUnit->Append(_("Minutes"));
    m_rangeUnit->Append(_("Hours"));
    m_rangeUnit->Append(_("Days"));
    m_rangeUnit->SetSelection(2);  // 24 小时
    m_rangeValue->Bind(wxEVT_SPINCTRL, &HistoryDialog::OnRangeChanged, this);
    m_rangeValue->Bind(wxEVT_TEXT_ENTER, &HistoryDialog::OnRangeChanged, this);
    m_rangeValue->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
        wxCommandEvent dummy;
        OnRangeChanged(dummy);
        evt.Skip();
    });
    m_rangeUnit->Bind(wxEVT_CHOICE, &HistoryDialog::OnRangeChanged, this);

    auto* quickRow = new wxBoxSizer(wxHORIZONTAL);
    quickRow->Add(m_rangeValue, 1, wxEXPAND | wxRIGHT, 6);
    quickRow->Add(m_rangeUnit, 0, wxEXPAND);
    leftSizer->Add(quickRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

    auto* timeTitle = new wxStaticText(leftCard, wxID_ANY, _("Time Range"));
    timeTitle->SetFont(timeTitle->GetFont().Bold());
    timeTitle->SetForegroundColour(ui::kThemeDark);
    leftSizer->Add(timeTitle, 0, wxLEFT | wxRIGHT | wxTOP, 12);

    auto now = wxDateTime::Now();
    auto yesterday = now - wxDateSpan::Day();

    auto* startSizer = new wxBoxSizer(wxHORIZONTAL);
    startSizer->Add(new wxStaticText(leftCard, wxID_ANY, _("Start: ")), 0, wxALIGN_CENTER_VERTICAL);
    m_startDate = new wxDatePickerCtrl(leftCard, wxID_ANY, yesterday);
    m_startTime = new wxTimePickerCtrl(leftCard, wxID_ANY, yesterday);
    startSizer->Add(m_startDate, 1, wxEXPAND | wxRIGHT, 5);
    startSizer->Add(m_startTime, 1, wxEXPAND);
    leftSizer->Add(startSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

    auto* endSizer = new wxBoxSizer(wxHORIZONTAL);
    endSizer->Add(new wxStaticText(leftCard, wxID_ANY, _("End:   ")), 0, wxALIGN_CENTER_VERTICAL);
    m_endDate = new wxDatePickerCtrl(leftCard, wxID_ANY, now);
    m_endTime = new wxTimePickerCtrl(leftCard, wxID_ANY, now);
    endSizer->Add(m_endDate, 1, wxEXPAND | wxRIGHT, 5);
    endSizer->Add(m_endTime, 1, wxEXPAND);
    leftSizer->Add(endSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

    auto* btnQuery = new ui::FlatButton(leftCard, _("Query"), ui::kThemeBlue, *wxWHITE, [this]() {
        wxCommandEvent e;
        OnQuery(e);
    });
    leftSizer->Add(btnQuery, 0, wxEXPAND | wxALL, 12);

    leftCard->SetSizer(leftSizer);
    contentSizer->Add(leftCard, 1, wxEXPAND | wxRIGHT, 10);

    // --- 右侧：图表卡片 ---
    auto* rightCard = new ui::CardPanel(this);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    m_chart = new ChartPanel(rightCard);
    // 图表默认有白底，所以嵌在卡片内看起来很自然
    rightSizer->Add(m_chart, 1, wxEXPAND | wxALL, 8);

    rightCard->SetSizer(rightSizer);
    contentSizer->Add(rightCard, 3, wxEXPAND);

    mainSizer->Add(contentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    // 底部关闭按钮
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnClose = new ui::FlatButton(this, _("Close"), ui::kBtnGrayBg, ui::kTextColor,
                                        [this]() { EndModal(wxID_CANCEL); });
    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnClose, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

    SetSizer(mainSizer);
    CentreOnParent();
}

void HistoryDialog::OnRangeChanged(wxCommandEvent&) {
    const int unit = m_rangeUnit->GetSelection();
    const int value = m_rangeValue->GetValue();
    if (unit < 0 || unit >= kTimeUnitCount || value <= 0) return;

    const double totalSeconds = static_cast<double>(value) * kTimeUnitMultipliers[unit];
    const wxDateTime end = wxDateTime::Now();
    wxDateTime start = end;
    start.Subtract(wxTimeSpan::Milliseconds(
        static_cast<wxLongLong>(static_cast<long long>(totalSeconds * 1000.0))));

    m_startDate->SetValue(start);
    m_startTime->SetValue(start);
    m_endDate->SetValue(end);
    m_endTime->SetValue(end);
}

void HistoryDialog::OnQuery(wxCommandEvent&) {
    std::vector<std::string> selectedVars;
    for (unsigned int i = 0; i < m_varList->GetCount(); ++i) {
        if (m_varList->IsChecked(i)) {
            selectedVars.push_back(std::string(m_varList->GetString(i).utf8_str()));
        }
    }

    if (selectedVars.empty()) {
        ui::MessageDialog::Show(this, _("Notice"), _("Please select at least one variable."),
                                wxOK | wxICON_INFORMATION);
        return;
    }

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

    long long startMs = start.GetValue().GetValue();
    long long endMs = end.GetValue().GetValue();

    double startSec = static_cast<double>(startMs) / 1000.0;
    double endSec = static_cast<double>(endMs) / 1000.0;

    if (endSec <= startSec) {
        ui::MessageDialog::Show(this, _("Notice"), _("End time must be later than start time."),
                                wxOK | wxICON_INFORMATION);
        return;
    }

    auto records = m_db->QueryHistory(m_projectPath, startSec, endSec);

    m_chart->Clear();
    m_chart->SetTimeWindow(endSec - startSec);

    int loadedCount = 0;
    for (const auto& rec : records) {
        if (std::find(selectedVars.begin(), selectedVars.end(), rec.varName) ==
            selectedVars.end()) {
            continue;
        }
        m_chart->AddDataPointWithTime(rec.varName, rec.value, rec.timeSec);
        loadedCount++;
    }

    if (loadedCount == 0) {
        ui::MessageDialog::Show(this, _("Notice"), _("No data found in this time range."),
                                wxOK | wxICON_INFORMATION);
    }
}

}  // namespace icu
