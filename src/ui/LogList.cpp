#include "LogList.h"

#include <wx/dc.h>
#include <wx/graphics.h>

#include <algorithm>

#include "UIUtils.h"

namespace icu {
namespace ui {

namespace {
const wxColour kSendColor(59, 130, 246);
const wxColour kReceiveColor(34, 197, 94);
const wxColour kInfoColor(148, 163, 184);
const wxColour kErrorColor(239, 68, 68);

wxString FitText(wxDC& dc, const wxString& text, int maxWidth) {
    if (text.empty() || maxWidth <= 0) return {};
    if (dc.GetTextExtent(text).GetWidth() <= maxWidth) return text;

    wxString result = text;
    while (!result.empty() && dc.GetTextExtent(result + "...").GetWidth() > maxWidth)
        result.RemoveLast();
    return result + "...";
}
}  // namespace

LogList::LogList(wxWindow* parent)
    : wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundColour(*wxWHITE);
    Bind(wxEVT_SHOW, [this](wxShowEvent& evt) {
        if (evt.IsShown() && m_autoScroll) ScrollToBottom();
        evt.Skip();
    });
    Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        evt.Skip();
        Refresh();
    });
}

void LogList::ScrollToBottom() {
    if (m_entries.empty()) return;
    ScrollToRow(m_entries.size() - 1);
    Refresh();
}

void LogList::Append(const wxString& time, Kind kind, const wxString& content,
                     const wxString& hex) {
    m_entries.push_back({time, content, hex, kind});

    // 长时间运行时限幅，避免无限增长
    if (m_entries.size() > 5000) m_entries.erase(m_entries.begin());

    SetItemCount(m_entries.size());
    if (m_autoScroll)
        ScrollToBottom();
    else
        Refresh();
}

void LogList::AppendMessage(const wxString& time, const wxString& content, bool error) {
    Append(time, error ? Kind::Error : Kind::Info, content, wxString());
}

void LogList::Clear() {
    m_entries.clear();
    SetItemCount(0);
    Refresh();
}

void LogList::SetShowTime(bool show) {
    m_showTime = show;
    Refresh();
}

void LogList::SetShowHex(bool show) {
    m_showHex = show;
    Refresh();
}

void LogList::SetAutoScroll(bool enabled) {
    m_autoScroll = enabled;
    if (m_autoScroll) ScrollToBottom();
}

wxCoord LogList::OnMeasureItem(size_t) const {
    return 34;
}

void LogList::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const {
    if (n >= m_entries.size()) return;
    const Entry& entry = m_entries[n];

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(n % 2 == 0 ? *wxWHITE : wxColour(249, 250, 251)));
    dc.DrawRectangle(rect);

    dc.SetPen(wxPen(wxColour(241, 245, 249), 1));
    dc.DrawLine(rect.x, rect.GetBottom(), rect.GetRight(), rect.GetBottom());

    dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    if (m_showTime) {
        dc.SetTextForeground(wxColour(100, 116, 139));
        dc.DrawText(entry.time, rect.x + kTimeX, rect.y + (rect.height - dc.GetCharHeight()) / 2);
    }

    wxString direction;
    wxColour badgeColor;
    switch (entry.kind) {
        case Kind::Send:
            direction = _("Send");
            badgeColor = kSendColor;
            break;
        case Kind::Receive:
            direction = _("Receive");
            badgeColor = kReceiveColor;
            break;
        case Kind::Error:
            direction = _("Error");
            badgeColor = kErrorColor;
            break;
        case Kind::Info:
        default:
            direction = _("Info");
            badgeColor = kInfoColor;
            break;
    }

    const int badgeX = rect.x + kDirectionX;
    const int badgeY = rect.y + (rect.height - 20) / 2;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(badgeColor));
    dc.DrawRoundedRectangle(badgeX, badgeY, kDirectionWidth, 20, 4);

    dc.SetTextForeground(*wxWHITE);
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    const wxSize badgeText = dc.GetTextExtent(direction);
    dc.DrawText(direction, badgeX + (kDirectionWidth - badgeText.GetWidth()) / 2,
                badgeY + (20 - badgeText.GetHeight()) / 2);

    const int contentX = rect.x + kContentX;
    const int contentRight = rect.GetRight() - 8;
    int contentWidth = contentRight - contentX;

    int hexWidth = 0;
    if (m_showHex && !entry.hex.empty()) {
        hexWidth = std::min(220, contentWidth / 3);
        contentWidth -= hexWidth + 12;
    }

    dc.SetTextForeground(wxColour(30, 41, 59));
    dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    const wxString content = FitText(dc, entry.content, contentWidth);
    dc.DrawText(content, contentX, rect.y + (rect.height - dc.GetCharHeight()) / 2);

    if (hexWidth > 0) {
        dc.SetTextForeground(wxColour(148, 163, 184));
        dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        const wxString hexText = FitText(dc, entry.hex, hexWidth);
        const int hexX = contentRight - dc.GetTextExtent(hexText).GetWidth();
        dc.DrawText(hexText, hexX, rect.y + (rect.height - dc.GetCharHeight()) / 2);
    }
}

}  // namespace ui
}  // namespace icu
