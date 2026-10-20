#pragma once

#include <wx/graphics.h>
#include <wx/wx.h>

#include <map>
#include <string>
#include <vector>

namespace icu {

struct ChartPoint {
    double y;
    double timeSec;
};

class ChartPanel : public wxPanel {
public:
    ChartPanel(wxWindow* parent);
    ~ChartPanel() override = default;

    void AddDataPoint(const std::string& varName, double value);
    void Clear();
    void SetTimeWindow(double seconds);
    void AddDataPointWithTime(const std::string& varName, double value, double timeSec);

private:
    void OnPaint(wxPaintEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseDoubleClick(wxMouseEvent& evt);  // ★ 新增：双击恢复自动跟随

    void DrawGrid(wxGraphicsContext* gc, const wxSize& size);
    void DrawLines(wxGraphicsContext* gc, const wxSize& size);
    void DrawYAxis(wxGraphicsContext* gc, const wxSize& size);
    void DrawXAxis(wxGraphicsContext* gc, const wxSize& size);

    struct Series {
        std::vector<ChartPoint> points;
        wxColour color;
    };

    std::map<std::string, Series> m_series;
    double m_minY = 0.0;
    double m_maxY = 100.0;

    // 视图控制
    double m_viewWindow = 10.0;  // 默认显示最近 10 秒
    double m_viewEnd = 0;
    bool m_autoFollow = true;  // ★ 新增：自动跟随最新数据

    bool m_isDragging = false;
    wxPoint m_lastMousePos;

    const wxColour COLORS[5] = {
        wxColour(0, 150, 255),  // 蓝
        wxColour(255, 100, 0),  // 橙
        wxColour(0, 200, 100),  // 绿
        wxColour(200, 0, 200),  // 紫
        wxColour(150, 100, 50)  // 棕
    };

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
