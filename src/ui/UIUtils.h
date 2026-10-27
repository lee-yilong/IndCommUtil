#pragma once

#include <wx/artprov.h>
#include <wx/checklst.h>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace icu {
namespace ui {

// ================= 统一的主题颜色 =================
inline const wxColour kThemeBlue(41, 182, 246);
inline const wxColour kThemeDark(38, 50, 56);
inline const wxColour kBgColor(243, 244, 246);
inline const wxColour kCardBgColor(255, 255, 255);
inline const wxColour kTextColor(50, 50, 50);
inline const wxColour kSubTextColor(120, 120, 120);
inline const wxColour kBorderColor(229, 231, 235);
inline const wxColour kSelectBgColor(239, 246, 255);
inline const wxColour kBtnGrayBg(229, 231, 235);
inline const wxColour kCloseHoverColor(239, 68, 68);

// ================= 通用 SVG 加载与着色 =================
inline wxBitmapBundle LoadSvgIcon(const wxString& name, int size = 16,
                                  wxColour tint = wxNullColour) {
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    wxString path = exePath.GetPath() + "/resources/icons/" + name + ".svg";
    if (wxFileExists(path)) {
        wxBitmapBundle bundle = wxBitmapBundle::FromSVGFile(path, wxSize(size, size));
        if (bundle.IsOk()) {
            if (tint.IsOk()) {
                wxBitmap bmp = bundle.GetBitmap(wxSize(size, size));
                if (bmp.IsOk()) {
                    wxImage img = bmp.ConvertToImage();
                    if (img.IsOk()) {
                        if (!img.HasAlpha()) {
                            img.InitAlpha();
                            unsigned char* alpha = img.GetAlpha();
                            unsigned char* data = img.GetData();
                            for (int i = 0; i < size * size; ++i) {
                                if (data[i * 3] > 250 && data[i * 3 + 1] > 250 &&
                                    data[i * 3 + 2] > 250) {
                                    alpha[i] = 0;
                                }
                            }
                        }
                        unsigned char* data = img.GetData();
                        unsigned char* alpha = img.GetAlpha();
                        for (int i = 0; i < size * size; ++i) {
                            if (alpha[i] > 10) {
                                data[i * 3] = tint.Red();
                                data[i * 3 + 1] = tint.Green();
                                data[i * 3 + 2] = tint.Blue();
                            }
                        }
                    }
                    return wxBitmapBundle::FromBitmap(wxBitmap(img));
                }
            }
            return bundle;
        }
    }
    return wxArtProvider::GetBitmapBundle(wxART_NORMAL_FILE, wxART_OTHER, wxSize(size, size));
}

// ================= 圆角卡片面板 =================
class CardPanel : public wxPanel {
public:
    CardPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kCardBgColor);
        Bind(wxEVT_PAINT, &CardPanel::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

            // ★ 修复问题 2：添加非常轻柔的投影，让卡片产生悬浮感，区分层级
            gc->SetBrush(wxBrush(wxColour(0, 0, 0, 12)));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawRoundedRectangle(1.5, 2.5, GetSize().x - 3, GetSize().y - 3, 8);

            // 绘制卡片主体
            gc->SetBrush(wxBrush(kCardBgColor));
            gc->SetPen(wxPen(kBorderColor, 1));
            gc->DrawRoundedRectangle(0.5, 0.5, GetSize().x - 3, GetSize().y - 3, 8);
        }
    }
};

// ================= 扁平化按钮 =================
class FlatButton : public wxPanel {
public:
    FlatButton(wxWindow* parent, const wxString& text, const wxColour& bg, const wxColour& fg,
               std::function<void()> onClick)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(80, 32), wxBORDER_NONE),
          m_text(text),
          m_bg(bg),
          m_fg(fg),
          m_onClick(onClick) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetCursor(wxCursor(wxCURSOR_HAND));

        Bind(wxEVT_PAINT, &FlatButton::OnPaint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
            if (m_enabled && m_onClick) m_onClick();
        });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) {
            m_hover = true;
            Refresh();
            e.Skip();
        });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
            m_hover = false;
            Refresh();
            e.Skip();
        });
    }

    void SetBtnText(const wxString& t) {
        m_text = t;
        Refresh();
    }
    void SetBtnEnabled(bool en) {
        m_enabled = en;
        m_bg = en ? kBtnGrayBg : wxColour(245, 245, 245);
        Refresh();
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            wxColour drawBg = m_hover && m_enabled ? m_bg.ChangeLightness(95) : m_bg;
            gc->SetBrush(wxBrush(drawBg));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawRoundedRectangle(0, 0, GetSize().x, GetSize().y, 4);

            gc->SetFont(GetFont(), m_enabled ? m_fg : kSubTextColor);
            double tw, th;
            gc->GetTextExtent(m_text, &tw, &th);
            gc->DrawText(m_text, (GetSize().x - tw) / 2, (GetSize().y - th) / 2);
        }
    }

    wxString m_text;
    wxColour m_bg, m_fg;
    bool m_hover = false;
    bool m_enabled = true;
    std::function<void()> m_onClick;
};

// ================= 窗口拖拽辅助 =================
inline void SetupDialogDrag(wxWindow* target, wxDialog* dialog, wxPoint& dragStart,
                            wxPoint& winStart) {
    target->Bind(wxEVT_LEFT_DOWN, [dialog, &dragStart, &winStart, target](wxMouseEvent& evt) {
        if (evt.LeftIsDown()) {
            dragStart = wxGetMousePosition();
            winStart = dialog->GetPosition();
            if (!target->HasCapture()) target->CaptureMouse();
        }
        evt.Skip();
    });
    target->Bind(wxEVT_MOTION, [dialog, &dragStart, &winStart, target](wxMouseEvent& evt) {
        if (evt.LeftIsDown() && target->HasCapture()) {
            wxPoint delta = wxGetMousePosition() - dragStart;
            dialog->Move(winStart + delta);
        }
        evt.Skip();
    });
    target->Bind(wxEVT_LEFT_UP, [target](wxMouseEvent& evt) {
        if (target->HasCapture()) target->ReleaseMouse();
        evt.Skip();
    });
    target->Bind(wxEVT_MOUSE_CAPTURE_LOST, [target](wxMouseCaptureLostEvent& evt) {
        if (target->HasCapture()) target->ReleaseMouse();
        evt.Skip();
    });
}

// ================= 生成统一的顶部标题栏 =================
inline wxPanel* CreateDialogHeader(wxDialog* dialog, const wxString& title, wxPoint& dragStart,
                                   wxPoint& winStart, std::function<void()> onClose) {
    auto* titlePanel = new wxPanel(dialog);
    titlePanel->SetBackgroundColour(kBgColor);
    auto* titleSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* titleText = new wxStaticText(titlePanel, wxID_ANY, title);
    wxFont titleFont = titleText->GetFont();
    titleFont.SetPointSize(12);
    titleFont.MakeBold();
    titleText->SetFont(titleFont);
    titleText->SetForegroundColour(kThemeDark);

    auto* closeBtn =
        new wxStaticBitmap(titlePanel, wxID_ANY, LoadSvgIcon("close", 16, kSubTextColor));
    closeBtn->SetCursor(wxCursor(wxCURSOR_HAND));
    closeBtn->Bind(wxEVT_LEFT_UP, [onClose](wxMouseEvent&) {
        if (onClose) onClose();
    });

    closeBtn->Bind(wxEVT_ENTER_WINDOW, [closeBtn](wxMouseEvent& evt) {
        closeBtn->SetBitmap(LoadSvgIcon("close", 16, kCloseHoverColor));
        evt.Skip();
    });
    closeBtn->Bind(wxEVT_LEAVE_WINDOW, [closeBtn](wxMouseEvent& evt) {
        closeBtn->SetBitmap(LoadSvgIcon("close", 16, kSubTextColor));
        evt.Skip();
    });

    titleSizer->AddStretchSpacer(1);
    titleSizer->Add(titleText, 0, wxALIGN_CENTER_VERTICAL);
    titleSizer->AddStretchSpacer(1);
    titleSizer->Add(closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    titlePanel->SetSizer(titleSizer);

    SetupDialogDrag(titlePanel, dialog, dragStart, winStart);
    SetupDialogDrag(titleText, dialog, dragStart, winStart);

    return titlePanel;
}

// ================= 主题化进度条 =================
class ProgressBar : public wxPanel {
public:
    explicit ProgressBar(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 22), wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(160, 22));
        Bind(wxEVT_PAINT, &ProgressBar::OnPaint, this);
    }

    void SetProgress(int value, int total) {
        m_total = std::max(0, total);
        m_value = std::max(0, std::min(value, m_total));
        Refresh();
    }

    int GetValue() const { return m_value; }
    int GetTotal() const { return m_total; }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;

        const wxSize size = GetClientSize();
        const double ratio = m_total > 0 ? static_cast<double>(m_value) / m_total : 0.0;

        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        gc->SetBrush(wxBrush(kBorderColor));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawRoundedRectangle(0, 0, size.x, size.y, 5);

        if (ratio > 0.0) {
            const double fillWidth = std::max(6.0, size.x * ratio);
            gc->SetBrush(wxBrush(wxColour(34, 197, 94)));
            gc->DrawRoundedRectangle(0, 0, fillWidth, size.y, 5);
        }

        const wxString text = wxString::Format("%.2f%%", ratio * 100.0);
        gc->SetFont(GetFont().Scaled(0.95), ratio >= 0.5 ? *wxWHITE : kTextColor);
        double tw = 0, th = 0;
        gc->GetTextExtent(text, &tw, &th);
        gc->DrawText(text, (size.x - tw) / 2, (size.y - th) / 2);
    }

    int m_value = 0;
    int m_total = 1;
};

// ================= 通用消息提示对话框 =================
class MessageDialog : public wxDialog {
public:
    static int Show(wxWindow* parent, const wxString& title, const wxString& msg,
                    long style = wxOK | wxICON_INFORMATION) {
        MessageDialog dlg(parent, title, msg, style);
        return dlg.ShowModal();
    }

private:
    MessageDialog(wxWindow* parent, const wxString& title, const wxString& msg, long style);
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

inline MessageDialog::MessageDialog(wxWindow* parent, const wxString& title, const wxString& msg,
                                    long style)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE | wxRESIZE_BORDER) {
    SetBackgroundColour(kBgColor);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* header = CreateDialogHeader(this, title, m_dragStart, m_winStart,
                                      [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    auto* card = new CardPanel(this);
    auto* cardSizer = new wxBoxSizer(wxHORIZONTAL);

    wxColour iconColor = kThemeBlue;
    if (style & wxICON_ERROR)
        iconColor = wxColour(239, 68, 68);
    else if (style & wxICON_WARNING)
        iconColor = wxColour(251, 191, 36);
    else if (style & wxICON_QUESTION)
        iconColor = kThemeBlue;

    auto* iconPanel = new wxPanel(card, wxID_ANY, wxDefaultPosition, wxSize(32, 32), wxBORDER_NONE);
    iconPanel->SetBackgroundColour(kCardBgColor);
    iconPanel->Bind(wxEVT_PAINT, [iconPanel, iconColor](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(iconPanel);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetBrush(wxBrush(iconColor));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawEllipse(2, 2, 28, 28);
            gc->SetFont(iconPanel->GetFont().Bold().Scaled(1.2), *wxWHITE);
            double tw, th;
            gc->GetTextExtent("!", &tw, &th);
            gc->DrawText("!", 16 - tw / 2, 16 - th / 2);
        }
    });
    cardSizer->Add(iconPanel, 0, wxALIGN_TOP | wxALL, 16);

    auto* text = new wxStaticText(card, wxID_ANY, msg);
    text->SetForegroundColour(kTextColor);
    text->SetFont(text->GetFont().Scaled(1.1));
    text->Wrap(FromDIP(460));
    cardSizer->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 16);

    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();

    auto addBtn = [&](const wxString& text, const wxColour& bg, const wxColour& fg, int id) {
        auto* btn = new FlatButton(this, text, bg, fg, [this, id]() { EndModal(id); });
        btnSizer->Add(btn, 0, wxLEFT, 10);
    };

    if ((style & wxYES_NO) == wxYES_NO) {
        if (style & wxCANCEL) addBtn(_("Cancel"), kBtnGrayBg, kTextColor, wxID_CANCEL);
        addBtn(_("No"), kBtnGrayBg, kTextColor, wxID_NO);
        addBtn(_("Yes"), kThemeBlue, *wxWHITE, wxID_YES);
    } else if (style & wxOK) {
        if (style & wxCANCEL) addBtn(_("Cancel"), kBtnGrayBg, kTextColor, wxID_CANCEL);
        addBtn(_("OK"), kThemeBlue, *wxWHITE, wxID_OK);
    }

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);
    SetSizerAndFit(mainSizer);
    SetMinSize(FromDIP(wxSize(380, 220)));
    CentreOnParent();
}

// ================= 通用多选列表对话框 =================
class CheckListDialog : public wxDialog {
public:
    static bool Show(wxWindow* parent, const wxString& title, const wxString& prompt,
                     const wxArrayString& choices, const std::vector<int>& initial,
                     std::vector<int>& selected) {
        CheckListDialog dlg(parent, title, prompt, choices, initial);
        if (dlg.ShowModal() != wxID_OK) return false;
        selected.clear();
        for (unsigned int i = 0; i < dlg.m_list->GetCount(); ++i) {
            if (dlg.m_list->IsChecked(i)) selected.push_back(static_cast<int>(i));
        }
        return true;
    }

private:
    CheckListDialog(wxWindow* parent, const wxString& title, const wxString& prompt,
                    const wxArrayString& choices, const std::vector<int>& initial)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                   wxBORDER_NONE | wxRESIZE_BORDER) {
        SetBackgroundColour(kBgColor);
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);

        auto* header = CreateDialogHeader(this, title, m_dragStart, m_winStart,
                                          [this]() { EndModal(wxID_CANCEL); });
        mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

        auto* card = new CardPanel(this);
        auto* cardSizer = new wxBoxSizer(wxVERTICAL);

        auto* promptText = new wxStaticText(card, wxID_ANY, prompt);
        promptText->SetForegroundColour(kTextColor);
        promptText->Wrap(FromDIP(420));
        cardSizer->Add(promptText, 0, wxEXPAND | wxALL, 16);

        m_list = new wxCheckListBox(card, wxID_ANY, wxDefaultPosition, wxSize(-1, 260), choices);
        for (int index : initial) {
            if (index >= 0 && index < static_cast<int>(m_list->GetCount()))
                m_list->Check(index, true);
        }
        cardSizer->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

        card->SetSizer(cardSizer);
        mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

        auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        btnSizer->AddStretchSpacer();
        auto* btnCancel = new FlatButton(this, _("Cancel"), kBtnGrayBg, kTextColor,
                                         [this]() { EndModal(wxID_CANCEL); });
        auto* btnOk =
            new FlatButton(this, _("OK"), kThemeBlue, *wxWHITE, [this]() { EndModal(wxID_OK); });
        btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
        btnSizer->Add(btnOk, 0);
        mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

        SetSizerAndFit(mainSizer);
        SetMinSize(FromDIP(wxSize(420, 460)));
        CentreOnParent();
    }

    wxCheckListBox* m_list = nullptr;
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

// ================= 通用关于对话框 =================
class AboutDialog : public wxDialog {
public:
    static void Show(wxWindow* parent, const wxString& appName, const wxString& desc) {
        AboutDialog dlg(parent, appName, desc);
        dlg.ShowModal();
    }

private:
    AboutDialog(wxWindow* parent, const wxString& appName, const wxString& desc);
    wxPoint m_dragStart;
    wxPoint m_winStart;
};

inline AboutDialog::AboutDialog(wxWindow* parent, const wxString& appName, const wxString& desc)
    : wxDialog(parent, wxID_ANY, _("About"), wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE | wxRESIZE_BORDER) {
    SetBackgroundColour(kBgColor);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* header = CreateDialogHeader(this, _("About"), m_dragStart, m_winStart,
                                      [this]() { EndModal(wxID_CANCEL); });
    mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    auto* card = new CardPanel(this);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(card, wxID_ANY, appName);
    wxFont tf = title->GetFont();
    tf.SetPointSize(18);
    tf.MakeBold();
    title->SetFont(tf);
    title->SetForegroundColour(kThemeDark);
    cardSizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 24);

    auto* descText = new wxStaticText(card, wxID_ANY, desc);
    descText->SetForegroundColour(kSubTextColor);
    cardSizer->Add(descText, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 10);

    auto* version = new wxStaticText(card, wxID_ANY, "v0.1.0");
    version->SetForegroundColour(kSubTextColor);
    cardSizer->Add(version, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 24);

    card->SetSizer(cardSizer);
    mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->AddStretchSpacer();
    auto* btnOk =
        new FlatButton(this, _("OK"), kThemeBlue, *wxWHITE, [this]() { EndModal(wxID_OK); });
    btnSizer->Add(btnOk, 0);
    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

    SetSizerAndFit(mainSizer);
    SetMinSize(FromDIP(wxSize(380, 300)));
    CentreOnParent();
}

}  // namespace ui
}  // namespace icu
