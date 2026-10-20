#include "ChartPanel.h"

#include <wx/datetime.h>
#include <wx/dcbuffer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace icu {

// clang-format off
wxBEGIN_EVENT_TABLE(ChartPanel, wxPanel)
    EVT_PAINT(ChartPanel::OnPaint)
    EVT_MOUSEWHEEL(ChartPanel::OnMouseWheel)
    EVT_LEFT_DOWN(ChartPanel::OnMouseDown)
    EVT_LEFT_UP(ChartPanel::OnMouseUp)
    EVT_MOTION(ChartPanel::OnMouseMove)
    EVT_LEFT_DCLICK(ChartPanel::OnMouseDoubleClick)
wxEND_EVENT_TABLE()
    // clang-format on

    ChartPanel::ChartPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(*wxWHITE);
    SetMinSize(wxSize(-1, 250));
}

void ChartPanel::AddDataPoint(const std::string& varName, double value) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    AddDataPointWithTime(varName, value, ms / 1000.0);
}

void ChartPanel::AddDataPointWithTime(const std::string& varName, double value, double timeSec) {
    if (m_series.find(varName) == m_series.end()) {
        int colorIdx = m_series.size() % 5;
        m_series[varName].color = COLORS[colorIdx];
    }

    auto& s = m_series[varName];
    s.points.push_back({value, timeSec});

    if (s.points.size() > 2000) {
        s.points.erase(s.points.begin());
    }

    m_minY = 1e9;
    m_maxY = -1e9;
    bool hasData = false;
    for (const auto& pair : m_series) {
        for (const auto& pt : pair.second.points) {
            if (pt.y < m_minY) m_minY = pt.y;
            if (pt.y > m_maxY) m_maxY = pt.y;
            hasData = true;
        }
    }

    if (hasData) {
        if (m_maxY - m_minY < 1e-6) {
            m_maxY += 10;
            m_minY -= 10;
        } else {
            double pad = (m_maxY - m_minY) * 0.1;
            m_maxY += pad;
            m_minY -= pad;
        }
    } else {
        m_minY = 0;
        m_maxY = 100;
    }

    if (m_autoFollow) {
        m_viewEnd = timeSec;
    }

    Refresh();
}

void ChartPanel::Clear() {
    m_series.clear();
    m_minY = 0.0;
    m_maxY = 100.0;
    m_viewEnd = 0;
    m_autoFollow = true;
    Refresh();
}

void ChartPanel::SetTimeWindow(double seconds) {
    m_viewWindow = seconds;
    m_autoFollow = true;
    Refresh();
}

void ChartPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) return;

    wxSize size = GetClientSize();
    int paddingLeft = 60;
    int paddingRight = 120;
    int paddingTop = 20;
    int paddingBottom = 40;

    int plotWidth = size.x - paddingLeft - paddingRight;
    int plotHeight = size.y - paddingTop - paddingBottom;

    if (plotWidth <= 0 || plotHeight <= 0) {
        delete gc;
        return;
    }

    gc->Translate(paddingLeft, paddingTop);
    gc->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL), *wxBLACK);

    DrawGrid(gc, wxSize(plotWidth, plotHeight));
    DrawYAxis(gc, wxSize(plotWidth, plotHeight));
    DrawXAxis(gc, wxSize(plotWidth, plotHeight));
    DrawLines(gc, wxSize(plotWidth, plotHeight));

    // 图例
    int legendX = plotWidth + 20;
    int legendY = 10;
    for (const auto& pair : m_series) {
        gc->SetPen(wxPen(pair.second.color, 3));
        gc->StrokeLine(legendX, legendY + 5, legendX + 20, legendY + 5);
        gc->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL),
                    *wxBLACK);
        gc->DrawText(wxString::FromUTF8(pair.first), legendX + 25, legendY);
        legendY += 25;
    }

    // 显示状态提示
    if (!m_autoFollow) {
        gc->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL),
                    *wxRED);
        gc->DrawText(_("(Paused - Double click to follow)"), 10, -15);
    }

    delete gc;
}

void ChartPanel::DrawGrid(wxGraphicsContext* gc, const wxSize& size) {
    gc->SetPen(wxPen(wxColour(230, 230, 230), 1));
    int ySteps = 5;
    for (int i = 0; i <= ySteps; ++i)
        gc->StrokeLine(0, size.y * i / ySteps, size.x, size.y * i / ySteps);
    int xSteps = 10;
    for (int i = 0; i <= xSteps; ++i)
        gc->StrokeLine(size.x * i / xSteps, 0, size.x * i / xSteps, size.y);
}

void ChartPanel::DrawYAxis(wxGraphicsContext* gc, const wxSize& size) {
    gc->SetPen(wxPen(*wxBLACK, 1));
    gc->StrokeLine(0, 0, 0, size.y);

    int ySteps = 5;
    for (int i = 0; i <= ySteps; ++i) {
        double y = size.y * i / ySteps;
        double val = m_maxY - (m_maxY - m_minY) * i / ySteps;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << val;
        wxString text = wxString::FromUTF8(oss.str());
        double tw, th;
        gc->GetTextExtent(text, &tw, &th);
        gc->DrawText(text, -tw - 5, y - th / 2);
    }
}

void ChartPanel::DrawXAxis(wxGraphicsContext* gc, const wxSize& size) {
    gc->SetPen(wxPen(*wxBLACK, 1));
    gc->StrokeLine(0, size.y, size.x, size.y);

    double maxTime = 0;
    bool hasData = false;
    for (const auto& pair : m_series) {
        if (!pair.second.points.empty()) {
            if (pair.second.points.back().timeSec > maxTime) {
                maxTime = pair.second.points.back().timeSec;
            }
            hasData = true;
        }
    }

    if (!hasData) {
        gc->DrawText(_("Waiting for data..."), size.x / 2 - 50, size.y + 5);
        return;
    }

    double viewEnd = m_autoFollow ? maxTime : (m_viewEnd == 0 ? maxTime : m_viewEnd);
    double viewStart = viewEnd - m_viewWindow;

    int xSteps = 5;
    for (int i = 0; i <= xSteps; ++i) {
        double x = size.x * i / xSteps;
        double ratio = (double)i / xSteps;

        double t = viewStart + m_viewWindow * ratio;

        auto tp = std::chrono::system_clock::from_time_t(0) +
                  std::chrono::milliseconds(static_cast<long long>(t * 1000));
        std::time_t time_t_val = std::chrono::system_clock::to_time_t(tp);
        wxDateTime dt(time_t_val);

        wxString text;
        if (m_viewWindow < 60) {
            auto ms = static_cast<long long>(t * 1000) % 1000;
            text = wxString::Format("%s.%03lld", dt.Format("%H:%M:%S"), ms);
        } else {
            text = dt.Format("%H:%M:%S");
        }

        double tw, th;
        gc->GetTextExtent(text, &tw, &th);
        gc->DrawText(text, x - tw / 2, size.y + 5);
    }

    wxString label = wxString::Format(_("Time (Window: %.1fs)"), m_viewWindow);
    double tw, th;
    gc->GetTextExtent(label, &tw, &th);
    gc->DrawText(label, size.x / 2 - tw / 2, size.y + 20);
}

void ChartPanel::DrawLines(wxGraphicsContext* gc, const wxSize& size) {
    double yRange = m_maxY - m_minY;
    if (yRange == 0) yRange = 1.0;

    double maxTime = 0;
    for (const auto& pair : m_series) {
        if (!pair.second.points.empty() && pair.second.points.back().timeSec > maxTime) {
            maxTime = pair.second.points.back().timeSec;
        }
    }

    double viewEnd = m_autoFollow ? maxTime : (m_viewEnd == 0 ? maxTime : m_viewEnd);
    double viewStart = viewEnd - m_viewWindow;

    gc->Clip(0, 0, size.x, size.y);

    for (const auto& pair : m_series) {
        const auto& pts = pair.second.points;
        if (pts.empty()) continue;

        gc->SetPen(wxPen(pair.second.color, 2));

        if (pts.size() == 1) {
            if (pts[0].timeSec >= viewStart && pts[0].timeSec <= viewEnd) {
                double x = ((pts[0].timeSec - viewStart) / m_viewWindow) * size.x;
                double y = size.y - ((pts[0].y - m_minY) / yRange) * size.y;
                gc->SetBrush(wxBrush(pair.second.color));
                gc->DrawEllipse(x - 4, y - 4, 8, 8);
            }
            continue;
        }

        wxGraphicsPath path = gc->CreatePath();
        bool started = false;

        for (size_t i = 0; i < pts.size(); ++i) {
            if (pts[i].timeSec < viewStart) continue;
            if (pts[i].timeSec > viewEnd) break;

            double x = ((pts[i].timeSec - viewStart) / m_viewWindow) * size.x;
            double y = size.y - ((pts[i].y - m_minY) / yRange) * size.y;

            if (!started) {
                path.MoveToPoint(x, y);
                started = true;
            } else {
                path.AddLineToPoint(x, y);
            }
        }
        if (started) gc->StrokePath(path);
    }
}

void ChartPanel::OnMouseWheel(wxMouseEvent& evt) {
    m_autoFollow = false;  // 缩放时暂停自动跟随
    int rot = evt.GetWheelRotation();
    if (rot > 0) {
        m_viewWindow *= 0.8;
    } else {
        m_viewWindow *= 1.25;
    }
    if (m_viewWindow < 0.1) m_viewWindow = 0.1;
    if (m_viewWindow > 3600 * 24) m_viewWindow = 3600 * 24;
    Refresh();
}

void ChartPanel::OnMouseDown(wxMouseEvent& evt) {
    m_isDragging = true;
    m_autoFollow = false;  // 拖拽时暂停自动跟随
    m_lastMousePos = evt.GetPosition();

    // 如果当前 m_viewEnd 还是跟随状态，记录一下当前位置
    if (m_viewEnd == 0) {
        for (const auto& pair : m_series) {
            if (!pair.second.points.empty() && pair.second.points.back().timeSec > m_viewEnd) {
                m_viewEnd = pair.second.points.back().timeSec;
            }
        }
    }

    CaptureMouse();
    Refresh();
}

void ChartPanel::OnMouseUp(wxMouseEvent& evt) {
    if (m_isDragging) {
        m_isDragging = false;
        if (HasCapture()) ReleaseMouse();
    }
}

void ChartPanel::OnMouseMove(wxMouseEvent& evt) {
    if (m_isDragging && evt.LeftIsDown()) {
        wxPoint pos = evt.GetPosition();
        int dx = pos.x - m_lastMousePos.x;
        m_lastMousePos = pos;

        if (dx != 0) {
            double pixelsPerSecond = (GetClientSize().x - 180) / m_viewWindow;
            m_viewEnd -= (dx / pixelsPerSecond);
            Refresh();
        }
    }
}

void ChartPanel::OnMouseDoubleClick(wxMouseEvent& evt) {
    // 双击恢复默认大小并开启自动跟随
    m_viewWindow = 10.0;
    m_autoFollow = true;
    m_viewEnd = 0;
    Refresh();
}

}  // namespace icu
