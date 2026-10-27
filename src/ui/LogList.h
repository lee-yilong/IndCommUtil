#pragma once

#include <wx/string.h>
#include <wx/vlbox.h>

#include <vector>

namespace icu {
namespace ui {

// 通信日志表格：时间 / 方向 / 内容
class LogList : public wxVListBox {
public:
    enum class Kind { Send, Receive, Info, Error };

    static constexpr int kTimeX = 10;
    static constexpr int kTimeWidth = 72;
    static constexpr int kDirectionX = 90;
    static constexpr int kDirectionWidth = 48;
    static constexpr int kContentX = 148;

    explicit LogList(wxWindow* parent);

    void Append(const wxString& time, Kind kind, const wxString& content,
                const wxString& hex = wxString());
    void AppendMessage(const wxString& time, const wxString& content, bool error = false);
    void Clear();

    void SetShowTime(bool show);
    void SetShowHex(bool show);
    void SetAutoScroll(bool enabled);

protected:
    void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const override;
    wxCoord OnMeasureItem(size_t n) const override;

private:
    struct Entry {
        wxString time;
        wxString content;
        wxString hex;
        Kind kind = Kind::Info;
    };

    std::vector<Entry> m_entries;
    bool m_showTime = true;
    bool m_showHex = true;
    bool m_autoScroll = true;
};

}  // namespace ui
}  // namespace icu
