#include "app/MainFrame.h"

#include <wx/artprov.h>
#include <wx/bmpbndl.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/icon.h>
#include <wx/msw/wrapwin.h>
#include <wx/popupwin.h>
#include <wx/radiobut.h>
#include <wx/statline.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>

#include <nlohmann/json.hpp>

#include "app/App.h"
#include "config/ConnectionProfile.h"
#include "core/IProtocol.h"
#include "core/ProtocolRegistry.h"
#include "core/StreamSession.h"
#include "log/Log.h"
#include "protocols/codec/RawCodec.h"
#include "settings/UISettings.h"
#include "spdlog/spdlog.h"
#include "ui/AppTaskBarIcon.h"
#include "ui/GenericProtocolDialog.h"
#include "ui/LogFrame.h"
#include "ui/SessionPanel.h"
#include "ui/SettingsDialog.h"
#include "ui/UIUtils.h"
#include "ui/WelcomePanel.h"

namespace icu {

namespace {
const wxColour kThemeBlue(56, 189, 248);
const wxColour kThemeOrange(251, 191, 36);
const wxColour kThemeDark(30, 41, 59);

const wxColour kBgColor(243, 244, 246);
const wxColour kTopBarColor(255, 255, 255);
const wxColour kCardBgColor(255, 255, 255);
const wxColour kSplitLineColor(209, 213, 219);

const wxColour kTextColor(51, 65, 85);
const wxColour kSubTextColor(100, 116, 139);
const wxColour kHoverBg(241, 245, 249);
const wxColour kCloseHoverColor(239, 68, 68);
const wxColour kSelectBgColor(239, 246, 255);

wxString SafeTrans(const wxString& text) {
    return text.IsEmpty() ? " " : text;
}

class MenuBtn : public wxPanel {
public:
    MenuBtn(wxWindow* parent, const wxString& iconName, const wxString& text)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
          m_iconName(iconName),
          m_text(text) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kTopBarColor);
        SetCursor(wxCursor(wxCURSOR_HAND));

        Bind(wxEVT_PAINT, &MenuBtn::OnPaint, this);
        Bind(wxEVT_ENTER_WINDOW, &MenuBtn::OnEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &MenuBtn::OnLeave, this);
        Bind(wxEVT_LEFT_UP, &MenuBtn::OnClick, this);

        CalculateSize();
    }

    void SetOpen(bool open) {
        if (m_isOpen != open) {
            m_isOpen = open;
            if (!open) m_hover = false;
            Refresh();
        }
    }

    void CalculateSize() {
        wxClientDC dc(this);
        wxFont font = GetFont();
        font.SetPointSize(11);
        dc.SetFont(font);
        wxSize ts = dc.GetTextExtent(m_text);
        SetMinSize(wxSize(40 + ts.x + 8 + 14 + 12, 36));
    }

    wxSize DoGetBestSize() const override {
        wxClientDC dc(const_cast<MenuBtn*>(this));
        wxFont font = GetFont();
        font.SetPointSize(11);
        dc.SetFont(font);
        wxSize ts = dc.GetTextExtent(m_text);
        return wxSize(40 + ts.x + 8 + 14 + 12, 36);
    }

private:
    wxString m_iconName;
    wxString m_text;
    bool m_hover = false;
    bool m_isOpen = false;

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(kTopBarColor));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        wxSize sz = GetSize();
        bool active = m_hover || m_isOpen;

        if (active) {
            gc->SetBrush(wxBrush(kSelectBgColor));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawRoundedRectangle(0, 0, sz.x, sz.y, 6);
        }

        wxColour col = active ? kThemeDark : kThemeDark;
        wxColour iconCol = active ? kThemeBlue : kThemeDark;

        wxBitmap bmp = ui::LoadSvgIcon(m_iconName, 20, iconCol).GetBitmap(wxSize(20, 20));
        gc->DrawBitmap(bmp, 12, (sz.y - 20) / 2, 20, 20);

        wxFont font = GetFont();
        font.SetPointSize(11);
        gc->SetFont(font, col);
        double tw, th;
        gc->GetTextExtent(m_text, &tw, &th);
        gc->DrawText(m_text, 40, (sz.y - th) / 2);

        wxBitmap arrBmp = ui::LoadSvgIcon(m_isOpen ? "arrow_up" : "arrow_down", 14,
                                          active ? kThemeBlue : kSubTextColor)
                              .GetBitmap(wxSize(14, 14));
        gc->DrawBitmap(arrBmp, 40 + tw + 8, (sz.y - 14) / 2, 14, 14);
    }

    void OnEnter(wxMouseEvent& e) {
        if (!m_hover) {
            m_hover = true;
            Refresh();
        }
        e.Skip();
    }
    void OnLeave(wxMouseEvent& e) {
        if (m_hover && !m_isOpen) {
            m_hover = false;
            Refresh();
        }
        e.Skip();
    }
    void OnClick(wxMouseEvent& e) {
        m_hover = true;
        m_isOpen = true;
        Refresh();
        wxCommandEvent evt(wxEVT_BUTTON, GetId());
        GetEventHandler()->ProcessEvent(evt);
        e.Skip();
    }
};

class LanguageDialog : public wxDialog {
public:
    LanguageDialog(wxWindow* parent, const wxString& curLang)
        : wxDialog(parent, wxID_ANY, _("Select Language"), wxDefaultPosition, wxDefaultSize,
                   wxBORDER_NONE | wxRESIZE_BORDER) {
        SetBackgroundColour(kBgColor);

        auto* mainSizer = new wxBoxSizer(wxVERTICAL);
        auto* header = ui::CreateDialogHeader(this, _("Select Language"), m_dragStart, m_winStart,
                                              [this]() { EndModal(wxID_CANCEL); });
        mainSizer->Add(header, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

        auto* card = new ui::CardPanel(this);
        auto* cardSizer = new wxBoxSizer(wxVERTICAL);

        auto* title = new wxStaticText(card, wxID_ANY, _("Please select your language:"));
        title->SetFont(title->GetFont().Bold());
        title->SetForegroundColour(kThemeDark);
        cardSizer->Add(title, 0, wxALL, 16);

        auto* radioBox =
            new wxRadioBox(card, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           {_("Simplified Chinese"), _("English")}, 1, wxRA_SPECIFY_COLS);
        radioBox->SetSelection(curLang.StartsWith("en") ? 1 : 0);
        m_radio = radioBox;
        cardSizer->Add(radioBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

        card->SetSizer(cardSizer);
        mainSizer->Add(card, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

        auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* btnCancel = new ui::FlatButton(this, _("Cancel"), ui::kBtnGrayBg, ui::kTextColor,
                                             [this]() { EndModal(wxID_CANCEL); });
        auto* btnOk = new ui::FlatButton(this, _("OK"), ui::kThemeBlue, *wxWHITE,
                                         [this]() { EndModal(wxID_OK); });

        btnSizer->AddStretchSpacer();
        btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
        btnSizer->Add(btnOk, 0);
        mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);

        SetSizerAndFit(mainSizer);
        SetMinSize(FromDIP(wxSize(320, 240)));
        CentreOnParent();
    }
    int GetSelection() const { return m_radio ? m_radio->GetSelection() : 0; }

private:
    wxRadioBox* m_radio = nullptr;
    wxPoint m_dragStart;
    wxPoint m_winStart;
};
}  // namespace

class CustomPopupMenu : public wxPopupTransientWindow {
public:
    CustomPopupMenu(wxWindow* parent, std::function<void()> onDismiss)
        : wxPopupTransientWindow(parent, wxBORDER_NONE), m_onDismiss(onDismiss) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        auto* panel = new wxPanel(this);
        panel->SetBackgroundColour(*wxWHITE);
        m_sizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(m_sizer);
        auto* topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(panel, 1, wxEXPAND | wxALL, 1);
        SetSizer(topSizer);
        m_rootPanel = panel;
    }

    void OnDismiss() override {
        wxPopupTransientWindow::OnDismiss();
        if (m_onDismiss) m_onDismiss();
    }

    void AddItem(const wxString& iconName, const wxString& label, const wxString& shortcut,
                 bool enabled, std::function<void()> action) {
        auto* itemPanel = new wxPanel(m_rootPanel);
        itemPanel->SetBackgroundColour(*wxWHITE);
        itemPanel->SetMinSize(wxSize(-1, 36));
        auto* hs = new wxBoxSizer(wxHORIZONTAL);
        wxColour txtCol = enabled ? kTextColor : wxColour(180, 180, 180);

        wxStaticBitmap* icon = nullptr;
        if (!iconName.IsEmpty()) {
            icon = new wxStaticBitmap(
                itemPanel, wxID_ANY,
                ui::LoadSvgIcon(iconName, 18, enabled ? kThemeDark : wxColour(180, 180, 180)));
            hs->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);
        } else {
            hs->AddSpacer(42);
        }

        auto* lbl = new wxStaticText(itemPanel, wxID_ANY, SafeTrans(label));
        lbl->SetForegroundColour(txtCol);
        wxFont lf = lbl->GetFont();
        lf.SetPointSize(10);
        lbl->SetFont(lf);
        hs->Add(lbl, 1, wxALIGN_CENTER_VERTICAL);

        if (!shortcut.IsEmpty()) {
            auto* sc = new wxStaticText(itemPanel, wxID_ANY, SafeTrans(shortcut));
            sc->SetForegroundColour(enabled ? kSubTextColor : wxColour(200, 200, 200));
            hs->Add(sc, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
        } else {
            hs->AddSpacer(16);
        }
        itemPanel->SetSizer(hs);
        m_sizer->Add(itemPanel, 0, wxEXPAND);
        if (!enabled) return;

        auto onEnter = [itemPanel, lbl, icon, iconName](wxMouseEvent& evt) {
            itemPanel->SetBackgroundColour(kHoverBg);
            lbl->SetForegroundColour(kThemeBlue);
            if (icon) icon->SetBitmap(ui::LoadSvgIcon(iconName, 18, kThemeBlue));
            itemPanel->Refresh();
            evt.Skip();
        };
        auto onLeave = [itemPanel, lbl, icon, iconName](wxMouseEvent& evt) {
            itemPanel->SetBackgroundColour(*wxWHITE);
            lbl->SetForegroundColour(kTextColor);
            if (icon) icon->SetBitmap(ui::LoadSvgIcon(iconName, 18, kThemeDark));
            itemPanel->Refresh();
            evt.Skip();
        };
        auto onClick = [this, action](wxMouseEvent& evt) {
            Dismiss();
            if (action) CallAfter(action);
        };

        itemPanel->Bind(wxEVT_ENTER_WINDOW, onEnter);
        itemPanel->Bind(wxEVT_LEAVE_WINDOW, onLeave);
        itemPanel->Bind(wxEVT_LEFT_UP, onClick);

        for (auto* child : itemPanel->GetChildren()) {
            child->Bind(wxEVT_ENTER_WINDOW, onEnter);
            child->Bind(wxEVT_LEAVE_WINDOW, onLeave);
            child->Bind(wxEVT_LEFT_UP, onClick);
        }
    }

    void AddSeparator() {
        auto* line = new wxPanel(m_rootPanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
        line->SetBackgroundColour(kHoverBg);
        m_sizer->Add(line, 0, wxEXPAND | wxTOP | wxBOTTOM, 6);
    }

    void ShowAt(wxWindow* anchor) {
        m_rootPanel->Layout();
        m_rootPanel->Fit();
        auto size = m_rootPanel->GetSize();
        SetSize(size.x + 2, size.y + 2);
        wxPoint pos = anchor->ClientToScreen(wxPoint(0, anchor->GetSize().y + 4));
        Position(pos, wxSize(0, 0));
        Popup();
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetClientSize();
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        dc.SetPen(wxPen(kHoverBg, 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(0, 0, size.x, size.y);
    }
    wxDECLARE_EVENT_TABLE();

private:
    wxPanel* m_rootPanel;
    wxBoxSizer* m_sizer;
    std::function<void()> m_onDismiss;
};
wxBEGIN_EVENT_TABLE(CustomPopupMenu, wxPopupTransientWindow) EVT_PAINT(CustomPopupMenu::OnPaint)
    wxEND_EVENT_TABLE()

    // ================= 标签实现 =================

    TabItem::TabItem(wxWindow* parent, const wxString& title, bool active,
                     std::function<void()> onSelect, std::function<void()> onClose)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
      m_active(active),
      m_onSelect(onSelect),
      m_onClose(onClose) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);

    m_text = new wxStaticText(this, wxID_ANY, title);
    m_text->SetForegroundColour(*wxBLACK);
    wxFont font = m_text->GetFont();
    font.SetPointSize(10);
    if (active) font.MakeBold();
    m_text->SetFont(font);
    sizer->Add(m_text, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);

    m_closeBtn = new wxStaticBitmap(this, wxID_ANY, ui::LoadSvgIcon("close", 12, kSubTextColor));
    m_closeBtn->SetCursor(wxCursor(wxCURSOR_HAND));
    m_closeBtn->SetToolTip(_("Close"));
    sizer->Add(m_closeBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);

    SetSizer(sizer);

    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (m_onSelect) m_onSelect();
        e.Skip();
    });
    m_text->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (m_onSelect) m_onSelect();
        e.Skip();
    });

    m_closeBtn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& e) {
        if (m_onClose) m_onClose();
    });
    m_closeBtn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& e) {
        m_closeBtn->SetBitmap(ui::LoadSvgIcon("close", 12, kCloseHoverColor));
        e.Skip();
    });
    m_closeBtn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& e) {
        m_closeBtn->SetBitmap(ui::LoadSvgIcon("close", 12, kSubTextColor));
        e.Skip();
    });

    Bind(wxEVT_PAINT, &TabItem::OnPaint, this);

    // 让标签文字/关闭按钮的背景色与标签底色一致，否则 STATIC 控件会画出白块
    UpdateColours();
}

void TabItem::UpdateColours() {
    const wxColour bg = m_active ? *wxWHITE : wxColour(235, 235, 235);
    SetBackgroundColour(bg);
    if (m_text) {
        m_text->SetBackgroundColour(bg);
        m_text->Refresh();
    }
    if (m_closeBtn) {
        m_closeBtn->SetBackgroundColour(bg);
        m_closeBtn->Refresh();
    }
    Refresh();
}

void TabItem::SetActive(bool active) {
    m_active = active;
    wxFont font = m_text->GetFont();
    if (active)
        font.MakeBold();
    else
        font.SetWeight(wxFONTWEIGHT_NORMAL);
    m_text->SetFont(font);
    UpdateColours();
}

void TabItem::SetTitle(const wxString& title) {
    if (!m_text) return;
    m_text->SetLabel(title);
    m_text->InvalidateBestSize();
    Layout();
}

void TabItem::SetClosable(bool closable) {
    if (!m_closeBtn) return;
    m_closeBtn->Show(closable);
    InvalidateBestSize();
    Layout();
    if (GetParent()) GetParent()->Layout();
}

void TabItem::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(kBgColor));
    dc.Clear();

    dc.SetPen(*wxTRANSPARENT_PEN);
    if (m_active) {
        dc.SetBrush(wxBrush(*wxWHITE));
        dc.DrawRectangle(0, 0, GetSize().x, GetSize().y);
    } else {
        dc.SetBrush(wxBrush(wxColour(235, 235, 235)));
        dc.DrawRectangle(0, 0, GetSize().x, GetSize().y);
    }
}

TabBook::TabBook(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(kBgColor);
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_tabBar = new wxPanel(this);
    m_tabBar->SetBackgroundColour(kBgColor);
    m_tabSizer = new wxBoxSizer(wxHORIZONTAL);
    m_tabBar->SetSizer(m_tabSizer);
    mainSizer->Add(m_tabBar, 0, wxEXPAND);

    m_book = new wxSimplebook(this);
    mainSizer->Add(m_book, 1, wxEXPAND);

    SetSizer(mainSizer);
}

void TabBook::BindPageChanged(std::function<void(int)> cb) {
    m_onPageChanged = cb;
}

void TabBook::AddPage(wxWindow* page, const wxString& text, bool select) {
    // wxSimplebook 只负责显示/隐藏页面，不会自动把页面挂到自己名下；
    // 页面必须先成为 m_book 的子窗口，否则它会以 TabBook 为坐标原点覆盖标签栏，
    // 而且会被 m_book 窗口挡住，导致页面里的控件完全收不到鼠标事件。
    if (page && page->GetParent() != m_book) page->Reparent(m_book);

    m_book->AddPage(page, text);
    auto* tab = new TabItem(
        m_tabBar, text, false, [this, page]() { SetSelection(GetPageIndex(page)); },
        [this, page]() {
            if (m_onClose) m_onClose(GetPageIndex(page));
        });
    m_tabSizer->Add(tab, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxLEFT, 4);
    m_pages.push_back({page, tab});
    m_tabBar->Layout();
    UpdateCloseButtons();

    if (select || m_pages.size() == 1) {
        SetSelection(m_pages.size() - 1);
    }
}

void TabBook::InsertPage(size_t index, wxWindow* page, const wxString& text, bool select) {
    if (page && page->GetParent() != m_book) page->Reparent(m_book);

    m_book->InsertPage(index, page, text);
    auto* tab = new TabItem(
        m_tabBar, text, false, [this, page]() { SetSelection(GetPageIndex(page)); },
        [this, page]() {
            if (m_onClose) m_onClose(GetPageIndex(page));
        });
    m_tabSizer->Insert(index, tab, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxLEFT, 4);
    m_pages.insert(m_pages.begin() + index, {page, tab});
    m_tabBar->Layout();
    UpdateCloseButtons();

    if (select) {
        SetSelection(index);
    }
}

void TabBook::DeletePage(size_t index) {
    if (index >= m_pages.size()) return;
    m_book->DeletePage(index);
    m_tabSizer->Detach(m_pages[index].tab);
    m_pages[index].tab->Destroy();
    m_pages.erase(m_pages.begin() + index);
    m_tabBar->Layout();
    UpdateCloseButtons();

    int sel = m_book->GetSelection();
    for (size_t i = 0; i < m_pages.size(); ++i) {
        m_pages[i].tab->SetActive(i == sel);
    }
    if (m_onPageChanged) m_onPageChanged(sel);
}

void TabBook::RemovePage(size_t index) {
    if (index >= m_pages.size()) return;
    m_book->RemovePage(index);
    m_tabSizer->Detach(m_pages[index].tab);
    m_pages[index].tab->Destroy();
    m_pages.erase(m_pages.begin() + index);
    m_tabBar->Layout();
    UpdateCloseButtons();

    int sel = m_book->GetSelection();
    for (size_t i = 0; i < m_pages.size(); ++i) {
        m_pages[i].tab->SetActive(i == sel);
    }
}

void TabBook::UpdateCloseButtons() {
    // 只剩一个页面时不允许关闭，隐藏关闭按钮避免“点了没反应”
    const bool closable = (m_pages.size() > 1);
    for (auto& entry : m_pages) {
        if (entry.tab) entry.tab->SetClosable(closable);
    }
}

void TabBook::SetSelection(size_t index) {
    if (index >= m_pages.size()) return;
    m_book->SetSelection(index);
    for (size_t i = 0; i < m_pages.size(); ++i) {
        m_pages[i].tab->SetActive(i == index);
    }
    if (m_onPageChanged) m_onPageChanged(index);
}

int TabBook::GetSelection() const {
    return m_book->GetSelection();
}
size_t TabBook::GetPageCount() const {
    return m_book->GetPageCount();
}
wxWindow* TabBook::GetPage(size_t index) const {
    return m_book->GetPage(index);
}

int TabBook::GetPageIndex(wxWindow* page) const {
    for (size_t i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i].window == page) return i;
    }
    return wxNOT_FOUND;
}

void TabBook::SetPageText(size_t index, const wxString& text) {
    if (index >= m_pages.size()) return;
    m_book->SetPageText(index, text);
    // 只更新标题文字，不能销毁并重建标签里的子控件，
    // 否则关闭按钮（以及它的点击绑定）会一起消失。
    m_pages[index].tab->SetTitle(text);
    m_tabBar->Layout();
}

// ================= 主窗口和菜单 =================

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame) EVT_CLOSE(MainFrame::OnClose) EVT_SIZE(MainFrame::OnSize)
    EVT_MENU(idMenuQuit, MainFrame::OnQuit) EVT_MENU(idMenuAbout, MainFrame::OnAbout) EVT_MENU(
        idMenuLog, MainFrame::OnShowLog) EVT_MENU(idMenuSettingsOptions, MainFrame::OnShowSettings)
        EVT_MENU(idMenuOpenConnection, MainFrame::OnOpenConnection)
            EVT_MENU(idMenuSaveConnection,
                     MainFrame::OnSaveConnection) EVT_MENU(idMenuSaveConnectionAs,
                                                           MainFrame::OnSaveConnectionAs)
                EVT_MENU(idMenuCloseConnection,
                         MainFrame::OnCloseConnection) EVT_MENU_RANGE(idMenuNewProtocolBase,
                                                                      idMenuNewProtocolEnd,
                                                                      MainFrame::OnMenuOpenProtocol)
                    EVT_MENU(idMenuLangZhCN, MainFrame::OnLangZhCN) EVT_MENU(idMenuLangEnUS,
                                                                             MainFrame::OnLangEnUS)
                        EVT_MENU_RANGE(idMenuRecentBase, idMenuRecentBase + 100,
                                       MainFrame::OnRecentConnection)
                            EVT_MENU(idMenuEditAdd, MainFrame::OnEditAdd)
                                EVT_MENU(idMenuEditCopy, MainFrame::OnEditCopy)
                                    EVT_MENU(idMenuEditEdit, MainFrame::OnEditEdit)
                                        EVT_MENU(idMenuEditDelete, MainFrame::OnEditDelete)
                                            EVT_BUTTON(wxID_NEW, MainFrame::OnMenuOpenProtocol)
                                                EVT_BUTTON(wxID_OPEN, MainFrame::OnOpenConnection)
                                                    wxEND_EVENT_TABLE()

                                                        MainFrame::MainFrame(wxFrame* parent,
                                                                             const wxString& title)
    : wxFrame(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE &
                  ~(wxCAPTION | wxSYSTEM_MENU | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)) {
    SetBackgroundColour(kBgColor);

#ifdef _WIN32
    HICON hIcon = static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                  IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    if (hIcon) {
        wxIcon icon;
        icon.CreateFromHICON(hIcon);
        SetIcon(icon);
    }
#endif

    m_maxIcon = ui::LoadSvgIcon("max", 20, kSubTextColor);
    m_restoreIcon = ui::LoadSvgIcon("restore", 20, kSubTextColor);

    BuildLayout();

    UpdateUIState();
    LoadRecentConnections();

    m_welcomePage = new WelcomePanel(m_notebook, m_recentConnections);
    m_welcomePage->m_onNewClick = [this]() {
        wxCommandEvent e(wxEVT_MENU, wxID_NEW);
        OnMenuOpenProtocol(e);
    };
    m_welcomePage->m_onOpenClick = [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuOpenConnection);
        OnOpenConnection(e);
    };
    m_welcomePage->m_onRecentClick = [this](int index) {
        wxCommandEvent e(wxEVT_MENU, idMenuRecentBase + index);
        OnRecentConnection(e);
    };

    m_notebook->AddPage(m_welcomePage, _("Welcome"), true);
    m_activeTabIndex = -1;

    const wxSize kDefaultSize = FromDIP(wxSize(1400, 900));
    const wxSize kMinSize = FromDIP(wxSize(900, 600));

    bool restored = false;
    const auto geo = settings::UISettings::Instance().LoadMainWindowGeometry();
    if (geo.valid && wxDisplay().GetClientArea().Intersects(geo.rect)) {
        SetSize(geo.rect);
        if (geo.maximized) Maximize(true);
        restored = true;
    }
    if (!restored) {
        SetClientSize(kDefaultSize);
        Centre();
    }
    SetMinClientSize(kMinSize);

    m_trayIcon = new AppTaskBarIcon();
    m_trayIcon->OnShow([this]() { RestoreFromTray(); });
    m_trayIcon->OnExit([this]() {
        m_forceExit = true;
        CallAfter([this]() { Close(true); });
    });

    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& evt) {
        if (!evt.GetActive()) {
            if (auto* btn = dynamic_cast<MenuBtn*>(m_fileBtn)) btn->SetOpen(false);
            if (auto* btn = dynamic_cast<MenuBtn*>(m_editBtn)) btn->SetOpen(false);
        }
        evt.Skip();
    });

    SPDLOG_INFO(_("IndComm started"));
}

MainFrame::~MainFrame() {
    for (auto& tab : m_tabs) {
        if (tab->session) {
            tab->session->DetachAllCallbacks();
            tab->session->Close();
        }
    }
    m_tabs.clear();
    SPDLOG_INFO(_("MainFrame destroyed"));
}

void MainFrame::SetupDrag(wxWindow* target) {
    target->Bind(wxEVT_LEFT_DOWN, [this, target](wxMouseEvent& evt) {
        if (evt.LeftIsDown()) {
            m_dragStart = wxGetMousePosition();
            m_winStart = GetPosition();
            if (!target->HasCapture()) target->CaptureMouse();
        }
        evt.Skip();
    });
    target->Bind(wxEVT_MOTION, [this, target](wxMouseEvent& evt) {
        if (evt.LeftIsDown() && target->HasCapture()) {
            wxPoint delta = wxGetMousePosition() - m_dragStart;
            Move(m_winStart + delta);
        }
        evt.Skip();
    });
    target->Bind(wxEVT_LEFT_UP, [target](wxMouseEvent& evt) {
        if (target->HasCapture()) target->ReleaseMouse();
        evt.Skip();
    });
    target->Bind(wxEVT_LEFT_DCLICK, [this, target](wxMouseEvent& evt) {
        if (target->HasCapture()) target->ReleaseMouse();
        Maximize(!IsMaximized());
        evt.Skip();
    });
    target->Bind(wxEVT_MOUSE_CAPTURE_LOST, [target](wxMouseCaptureLostEvent& evt) {
        if (target->HasCapture()) target->ReleaseMouse();
        evt.Skip();
    });
}

wxPanel* MainFrame::CreateDropdownBtn(wxWindow* parent, const wxString& iconName,
                                      const wxString& text, wxStaticBitmap** outArrow) {
    auto* btn = new MenuBtn(parent, iconName, text);
    btn->CalculateSize();
    *outArrow = nullptr;
    return btn;
}

void MainFrame::ShowFileMenu(wxWindow* anchor) {
    if (auto* btn = dynamic_cast<MenuBtn*>(m_fileBtn)) btn->SetOpen(true);

    auto* popup = new CustomPopupMenu(this, [this]() {
        if (auto* btn = dynamic_cast<MenuBtn*>(m_fileBtn)) btn->SetOpen(false);
    });

    bool hasTab = (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size());
    popup->AddItem("connect", _("New"), "", true, [this]() {
        wxCommandEvent e(wxEVT_MENU, wxID_NEW);
        OnMenuOpenProtocol(e);
    });
    if (!m_recentConnections.IsEmpty()) {
        for (size_t i = 0; i < m_recentConnections.GetCount(); ++i) {
            wxFileName fn(m_recentConnections[i]);
            wxString name = fn.GetName();
            if (name.Length() > 24) name = name.Left(21) + "...";
            popup->AddItem("history", name, "", true, [this, i]() {
                wxCommandEvent e(wxEVT_MENU, idMenuRecentBase + i);
                OnRecentConnection(e);
            });
        }
    } else {
        popup->AddItem("history", _("(None)"), "", false, nullptr);
    }
    popup->AddSeparator();
    popup->AddItem("open", _("Open"), "Ctrl+O", true, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuOpenConnection);
        OnOpenConnection(e);
    });
    popup->AddItem("record", _("Save"), "Ctrl+S", hasTab, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuSaveConnection);
        OnSaveConnection(e);
    });
    popup->AddItem("record", _("Save As"), "Ctrl+Shift+S", hasTab, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuSaveConnectionAs);
        OnSaveConnectionAs(e);
    });
    popup->AddItem("disconnect", _("Close"), "Ctrl+W", hasTab, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuCloseConnection);
        OnCloseConnection(e);
    });
    popup->AddSeparator();
    popup->AddItem("close", _("Quit"), "Alt+F4", true, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuQuit);
        OnQuit(e);
    });
    popup->ShowAt(anchor);
}

void MainFrame::ShowEditMenu(wxWindow* anchor) {
    if (auto* btn = dynamic_cast<MenuBtn*>(m_editBtn)) btn->SetOpen(true);

    auto* popup = new CustomPopupMenu(this, [this]() {
        if (auto* btn = dynamic_cast<MenuBtn*>(m_editBtn)) btn->SetOpen(false);
    });

    bool hasTab = (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size());
    bool hasSelection = false;
    auto* tab = GetActiveTab();
    if (hasTab && tab && tab->panel && tab->panel->HasSelection()) {
        hasSelection = true;
    }

    popup->AddItem("add", _("Add"), "Ctrl+N", hasTab, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuEditAdd);
        OnEditAdd(e);
    });
    popup->AddItem("copy", _("Copy"), "Ctrl+C", hasSelection, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuEditCopy);
        OnEditCopy(e);
    });
    popup->AddItem("edit", _("Edit"), "Ctrl+E", hasSelection, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuEditEdit);
        OnEditEdit(e);
    });
    popup->AddItem("delete", _("Delete"), "Del", hasSelection, [this]() {
        wxCommandEvent e(wxEVT_MENU, idMenuEditDelete);
        OnEditDelete(e);
    });
    popup->ShowAt(anchor);
}

void MainFrame::BuildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* row1 = new wxPanel(this);
    row1->SetBackgroundColour(kBgColor);
    row1->SetMinSize(wxSize(-1, 40));
    auto* row1Sizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftSpacer = new wxPanel(row1);
    leftSpacer->SetBackgroundColour(kBgColor);
    SetupDrag(leftSpacer);

    auto* centerPanel = new wxPanel(row1);
    centerPanel->SetBackgroundColour(kBgColor);
    auto* centerSizer = new wxBoxSizer(wxHORIZONTAL);

    wxBitmapBundle appIcon;
#ifdef _WIN32
    HICON hIcon = static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                  IMAGE_ICON, 20, 20, LR_DEFAULTCOLOR));
    if (hIcon) {
        wxIcon icon;
        icon.CreateFromHICON(hIcon);
        appIcon = wxBitmapBundle::FromBitmap(wxBitmap(icon));
    }
#endif
    if (appIcon.IsOk()) {
        auto* iconCtrl = new wxStaticBitmap(centerPanel, wxID_ANY, appIcon);
        centerSizer->Add(iconCtrl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        SetupDrag(iconCtrl);
    }

    auto* appName = new wxStaticText(centerPanel, wxID_ANY, "IndComm");
    wxFont f = appName->GetFont();
    f.SetPointSize(12);
    f.MakeBold();
    appName->SetFont(f);
    appName->SetForegroundColour(kThemeDark);
    centerSizer->Add(appName, 0, wxALIGN_CENTER_VERTICAL);

    m_centerTitleText = new wxStaticText(centerPanel, wxID_ANY, "");
    wxFont subF = m_centerTitleText->GetFont();
    subF.SetPointSize(11);
    m_centerTitleText->SetFont(subF);
    m_centerTitleText->SetForegroundColour(kSubTextColor);
    centerSizer->Add(m_centerTitleText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

    centerPanel->SetSizer(centerSizer);
    SetupDrag(centerPanel);
    SetupDrag(appName);
    SetupDrag(m_centerTitleText);

    auto* rightCtrlContainer = new wxPanel(row1);
    rightCtrlContainer->SetBackgroundColour(kBgColor);
    auto* rightCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
    rightCtrlSizer->AddStretchSpacer(1);

    auto* winCtrlPanel = new wxPanel(rightCtrlContainer);
    winCtrlPanel->SetBackgroundColour(kBgColor);
    auto* winCtrlSizer = new wxBoxSizer(wxHORIZONTAL);

    auto createWinBtn = [this, winCtrlPanel](const wxString& iconName, const wxString& tooltip,
                                             bool isClose, std::function<void()> action) {
        auto* btn = new wxStaticBitmap(winCtrlPanel, wxID_ANY,
                                       ui::LoadSvgIcon(iconName, 20, kSubTextColor));
        btn->SetToolTip(SafeTrans(tooltip));
        btn->SetCursor(wxCursor(wxCURSOR_HAND));
        btn->Bind(wxEVT_LEFT_UP, [action](wxMouseEvent&) { action(); });

        btn->Bind(wxEVT_ENTER_WINDOW, [btn, isClose](wxMouseEvent& evt) {
            btn->SetBackgroundColour(isClose ? kCloseHoverColor : kHoverBg);
            btn->Refresh();
            evt.Skip();
        });
        btn->Bind(wxEVT_LEAVE_WINDOW, [btn](wxMouseEvent& evt) {
            btn->SetBackgroundColour(kBgColor);
            btn->Refresh();
            evt.Skip();
        });
        return btn;
    };

    winCtrlSizer->Add(createWinBtn("min", _("Minimize"), false, [this]() { Iconize(true); }), 0,
                      wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);
    m_maxBtn = createWinBtn("max", _("Maximize"), false, [this]() { Maximize(!IsMaximized()); });
    winCtrlSizer->Add(m_maxBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);
    winCtrlSizer->Add(createWinBtn("close", _("Close"), true, [this]() { Close(); }), 0,
                      wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);
    winCtrlPanel->SetSizer(winCtrlSizer);
    SetupDrag(winCtrlPanel);

    rightCtrlSizer->Add(winCtrlPanel, 0, wxALIGN_CENTER_VERTICAL);
    rightCtrlContainer->SetSizer(rightCtrlSizer);
    SetupDrag(rightCtrlContainer);

    row1Sizer->Add(leftSpacer, 1, wxEXPAND);
    row1Sizer->Add(centerPanel, 0, wxALIGN_CENTER_VERTICAL);
    row1Sizer->Add(rightCtrlContainer, 1, wxEXPAND);

    row1->SetSizer(row1Sizer);
    SetupDrag(row1);
    root->Add(row1, 0, wxEXPAND);

    auto* row2 = new wxPanel(this);
    row2->SetBackgroundColour(kTopBarColor);
    row2->SetMinSize(wxSize(-1, 48));
    auto* row2Sizer = new wxBoxSizer(wxHORIZONTAL);

    auto* menuPanel = new wxPanel(row2);
    menuPanel->SetBackgroundColour(kTopBarColor);
    SetupDrag(menuPanel);
    auto* menuSizer = new wxBoxSizer(wxHORIZONTAL);

    m_fileBtn = CreateDropdownBtn(menuPanel, "connect", _("File"), &m_fileArrow);
    m_fileBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowFileMenu(m_fileBtn); });
    menuSizer->Add(m_fileBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);

    m_editBtn = CreateDropdownBtn(menuPanel, "edit", _("Edit"), &m_editArrow);
    m_editBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowEditMenu(m_editBtn); });
    menuSizer->Add(m_editBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);

    menuPanel->SetSizer(menuSizer);
    row2Sizer->Add(menuPanel, 0, wxEXPAND);

    auto* row2Spacer = new wxPanel(row2);
    row2Spacer->SetBackgroundColour(kTopBarColor);
    SetupDrag(row2Spacer);
    row2Sizer->Add(row2Spacer, 1, wxEXPAND);

    auto* statusIconsPanel = new wxPanel(row2);
    statusIconsPanel->SetBackgroundColour(kTopBarColor);
    auto* statusIconsSizer = new wxBoxSizer(wxHORIZONTAL);

    auto createStatusIcon = [this, statusIconsPanel](const wxString& iconName,
                                                     const wxString& tooltip,
                                                     std::function<void()> action) {
        auto* btn = new wxStaticBitmap(statusIconsPanel, wxID_ANY,
                                       ui::LoadSvgIcon(iconName, 22, kThemeDark));
        btn->SetToolTip(SafeTrans(tooltip));
        btn->SetCursor(wxCursor(wxCURSOR_HAND));
        btn->Bind(wxEVT_LEFT_UP, [action](wxMouseEvent&) { action(); });

        btn->Bind(wxEVT_ENTER_WINDOW, [btn, iconName](wxMouseEvent& evt) {
            btn->SetBitmap(ui::LoadSvgIcon(iconName, 22, kThemeBlue));
            btn->Refresh();
            evt.Skip();
        });
        btn->Bind(wxEVT_LEAVE_WINDOW, [btn, iconName](wxMouseEvent& evt) {
            btn->SetBitmap(ui::LoadSvgIcon(iconName, 22, kThemeDark));
            btn->Refresh();
            evt.Skip();
        });
        return btn;
    };

    statusIconsSizer->Add(createStatusIcon("log", _("Show Log"),
                                           [this]() {
                                               wxCommandEvent e;
                                               OnShowLog(e);
                                           }),
                          0, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);
    statusIconsSizer->Add(
        createStatusIcon("languag", _("Language"), [this]() { OnShowLanguageDialog(); }), 0,
        wxALIGN_CENTER_VERTICAL | wxLEFT, 16);
    statusIconsSizer->Add(createStatusIcon("setting", _("Settings"),
                                           [this]() {
                                               wxCommandEvent e;
                                               OnShowSettings(e);
                                           }),
                          0, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);
    statusIconsSizer->Add(createStatusIcon("about", _("About"),
                                           [this]() {
                                               wxCommandEvent e;
                                               OnAbout(e);
                                           }),
                          0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 20);

    statusIconsPanel->SetSizer(statusIconsSizer);
    SetupDrag(statusIconsPanel);
    row2Sizer->Add(statusIconsPanel, 0, wxEXPAND);

    row2->SetSizer(row2Sizer);
    SetupDrag(row2);
    root->Add(row2, 0, wxEXPAND);

    auto* shadowPanel =
        new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 6), wxBORDER_NONE);
    shadowPanel->SetBackgroundStyle(wxBG_STYLE_PAINT);
    shadowPanel->Bind(wxEVT_PAINT, [](wxPaintEvent& e) {
        wxWindow* win = static_cast<wxWindow*>(e.GetEventObject());
        wxAutoBufferedPaintDC dc(win);
        dc.SetBackground(wxBrush(kBgColor));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            wxGraphicsGradientStops stops(wxColour(0, 0, 0, 10), wxColour(0, 0, 0, 0));
            wxGraphicsBrush brush = gc->CreateLinearGradientBrush(0, 0, 0, 6, stops);
            gc->SetBrush(brush);
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawRectangle(0, 0, win->GetSize().x, 6);
        }
    });
    root->Add(shadowPanel, 0, wxEXPAND);

    auto* nbWrapper = new wxPanel(this);
    nbWrapper->SetBackgroundColour(kBgColor);
    auto* nbSizer = new wxBoxSizer(wxVERTICAL);

    m_notebook = new TabBook(nbWrapper);

    m_notebook->m_onClose = [this](int index) {
        const int welcomeIdx =
            m_welcomePage ? m_notebook->GetPageIndex(m_welcomePage) : wxNOT_FOUND;
        if (welcomeIdx != wxNOT_FOUND && index == welcomeIdx) {
            // 欢迎页也可以关闭，但至少保留一个页面
            CallAfter([this, index]() {
                if (m_notebook->GetPageCount() <= 1) return;
                if (m_welcomePage && m_notebook->GetPage(index) == m_welcomePage) {
                    m_notebook->RemovePage(index);
                    m_welcomePage->Destroy();
                    m_welcomePage = nullptr;
                    OnTabChanged(m_notebook->GetSelection());
                }
            });
            return;
        }

        CallAfter([this, index]() {
            int welcomeIdx = m_welcomePage ? m_notebook->GetPageIndex(m_welcomePage) : -1;
            int tabIdx = (welcomeIdx != wxNOT_FOUND && index > welcomeIdx) ? index - 1 : index;
            m_activeTabIndex = tabIdx;
            OnCloseConnection(*(new wxCommandEvent()));
        });
    };

    m_notebook->BindPageChanged([this](int index) { OnTabChanged(index); });

    nbSizer->Add(m_notebook, 1, wxEXPAND);
    nbWrapper->SetSizer(nbSizer);

    root->Add(nbWrapper, 1, wxEXPAND);

    auto* statusDivider = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    statusDivider->SetBackgroundColour(kSplitLineColor);
    statusDivider->SetMinSize(wxSize(-1, 1));
    root->Add(statusDivider, 0, wxEXPAND);

    m_statusPanel = new wxPanel(this);
    m_statusPanel->SetBackgroundColour(kTopBarColor);
    m_statusPanel->SetMinSize(wxSize(-1, FromDIP(32)));
    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);

    m_statusLeftText = new wxStaticText(m_statusPanel, wxID_ANY, _("Ready"));
    m_statusLeftText->SetForegroundColour(kTextColor);
    wxFont statusFont = m_statusLeftText->GetFont();
    statusFont.SetPointSize(9);
    m_statusLeftText->SetFont(statusFont);
    statusSizer->Add(m_statusLeftText, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 16);

    m_statusRightText = new wxStaticText(m_statusPanel, wxID_ANY, "");
    m_statusRightText->SetForegroundColour(kTextColor);
    m_statusRightText->SetFont(statusFont);
    statusSizer->Add(m_statusRightText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);

    m_statusPanel->SetSizer(statusSizer);
    root->Add(m_statusPanel, 0, wxEXPAND);

    SetSizer(root);
    root->Layout();
}

void MainFrame::OnShowLanguageDialog() {
    auto* app = static_cast<icu::App*>(wxTheApp);
    if (!app) return;
    LanguageDialog dlg(this, app->GetCurrentLanguage());
    if (dlg.ShowModal() == wxID_OK) {
        int sel = dlg.GetSelection();
        if (sel == 0)
            app->SwitchLanguage("zh_CN");
        else
            app->SwitchLanguage("en_US");
    }
}

void MainFrame::OnSize(wxSizeEvent& evt) {
    if (m_maxBtn) {
        if (IsMaximized()) {
            m_maxBtn->SetBitmap(m_restoreIcon);
            m_maxBtn->SetToolTip(_("Restore Down"));
        } else {
            m_maxBtn->SetBitmap(m_maxIcon);
            m_maxBtn->SetToolTip(_("Maximize"));
        }
        m_maxBtn->Refresh();
    }
    evt.Skip();
}

TabContext* MainFrame::GetActiveTab() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        return m_tabs[m_activeTabIndex].get();
    }
    return nullptr;
}

void MainFrame::SetupPanelEvents(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    auto* t = m_tabs[index].get();
    if (!t || !t->panel) return;
    auto* panel = t->panel;

    panel->OnRequestConfigure([this, index]() {
        CallAfter([this, index]() {
            if (index < 0 || index >= (int)m_tabs.size()) return;
            auto* tab = m_tabs[index].get();
            if (!tab->protocol || !tab->activeProfile) return;

            GenericProtocolDialog dlg(this, tab->protocol, tab->activeProfile->modeIndex,
                                      tab->activeProfile->fields);
            if (dlg.ShowModal() != wxID_OK) return;

            ActiveProfile newP;
            newP.protocolId = dlg.GetProtocolId();
            newP.modeIndex = dlg.GetModeIndex();
            newP.fields = dlg.GetValues();

            ISessionPtr newSession = tab->protocol->CreateSession(newP.modeIndex, newP.fields);
            if (!newSession) return;

            wxString pathCopy = tab->profilePath;
            int pageIdx = m_notebook->GetPageIndex(tab->panel);
            if (pageIdx == wxNOT_FOUND) return;

            if (tab->session) {
                tab->session->DetachAllCallbacks();
                tab->session->Close();
            }

            tab->session = std::move(newSession);
            tab->activeProfile = newP;

            auto* newPanel =
                new SessionPanel(m_notebook, tab->protocol, newP.modeIndex, tab->session.get(),
                                 &(tab->activeProfile->session), pathCopy);

            m_notebook->RemovePage(pageIdx);
            tab->panel->Destroy();
            tab->panel = nullptr;

            m_notebook->InsertPage(pageIdx, newPanel, GetDisplayName(index), true);
            m_notebook->SetSelection(pageIdx);
            tab->panel = newPanel;

            SetupPanelEvents(index);
            UpdateUIState();
            UpdateTabTitle(index);

            if (!tab->session->Open()) {
                SPDLOG_ERROR(_("Failed to open session"));
            }
        });
    });

    panel->OnStateUpdated([this, index](SessionState st) {
        if (index == m_activeTabIndex) {
            UpdateUIState();
        }
    });
}

void MainFrame::AddTab(IProtocol* proto, int modeIndex, ISessionPtr session,
                       const ActiveProfile& profile, const wxString& path) {
    auto tab = std::make_unique<TabContext>();
    tab->protocol = proto;
    tab->modeIndex = modeIndex;
    tab->session = std::move(session);
    tab->activeProfile = profile;

    if (!path.IsEmpty()) {
        tab->savedProfile = profile;
    }
    tab->profilePath = path;

    ISession* rawPtr = tab->session.get();
    tab->panel = new SessionPanel(m_notebook, proto, modeIndex, rawPtr,
                                  &(tab->activeProfile->session), path);

    int index = m_tabs.size();
    m_tabs.push_back(std::move(tab));

    m_notebook->AddPage(m_tabs[index]->panel, GetDisplayName(index), true);
    m_activeTabIndex = index;
    SetupPanelEvents(index);
    UpdateUIState();

    if (!rawPtr->Open()) {
        SPDLOG_ERROR(_("Failed to open session"));
    }
}

void MainFrame::RemoveTab(int index, bool skipNotebookDelete) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    auto& tab = m_tabs[index];
    if (tab->session) {
        tab->session->DetachAllCallbacks();
        tab->session->Close();
    }
    if (!skipNotebookDelete && tab->panel) {
        int pageIdx = m_notebook->GetPageIndex(tab->panel);
        if (pageIdx != wxNOT_FOUND) {
            m_notebook->DeletePage(pageIdx);
        }
    }
    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
        if (!m_welcomePage) {
            m_welcomePage = new WelcomePanel(m_notebook, m_recentConnections);
            m_welcomePage->m_onNewClick = [this]() {
                wxCommandEvent e(wxEVT_MENU, wxID_NEW);
                OnMenuOpenProtocol(e);
            };
            m_welcomePage->m_onOpenClick = [this]() {
                wxCommandEvent e(wxEVT_MENU, idMenuOpenConnection);
                OnOpenConnection(e);
            };
            m_welcomePage->m_onRecentClick = [this](int idx) {
                wxCommandEvent e(wxEVT_MENU, idMenuRecentBase + idx);
                OnRecentConnection(e);
            };
            m_notebook->AddPage(m_welcomePage, _("Welcome"), true);
        }
        int welcomeIdx = m_notebook->GetPageIndex(m_welcomePage);
        if (welcomeIdx != wxNOT_FOUND) m_notebook->SetSelection(welcomeIdx);
    } else {
        m_activeTabIndex = std::min(m_activeTabIndex, (int)m_tabs.size() - 1);
        if (m_activeTabIndex < 0) m_activeTabIndex = 0;

        int welcomeIdx = m_welcomePage ? m_notebook->GetPageIndex(m_welcomePage) : -1;
        int pageIdx = (welcomeIdx != wxNOT_FOUND) ? m_activeTabIndex + 1 : m_activeTabIndex;
        if (pageIdx >= 0 && pageIdx < (int)m_notebook->GetPageCount()) {
            m_notebook->SetSelection(pageIdx);
        }
    }
    UpdateUIState();
}

void MainFrame::OnTabChanged(int sel) {
    if (m_welcomePage && sel == m_notebook->GetPageIndex(m_welcomePage)) {
        m_activeTabIndex = -1;
    } else if (sel >= 0) {
        int welcomeIdx = m_welcomePage ? m_notebook->GetPageIndex(m_welcomePage) : -1;
        if (welcomeIdx != wxNOT_FOUND && sel > welcomeIdx) {
            m_activeTabIndex = sel - 1;
        } else {
            m_activeTabIndex = sel;
        }
    }
    UpdateUIState();
}

void MainFrame::UpdateUIState() {
    bool hasTab = (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size());

    if (!hasTab) {
        SetTitle(_("IndComm"));
        if (m_centerTitleText) m_centerTitleText->SetLabel("");
        if (m_statusLeftText) m_statusLeftText->SetLabel(_("Ready"));
        if (m_statusRightText) m_statusRightText->SetLabel("");
    } else {
        UpdateTabTitle(m_activeTabIndex);
        wxString title = GetDisplayName(m_activeTabIndex);
        if (IsModified(m_activeTabIndex)) title += " *";
        if (m_centerTitleText) m_centerTitleText->SetLabel(" - " + title);

        Layout();
        auto* tab = GetActiveTab();
        if (tab && tab->session) {
            SessionState st = tab->session->State();
            if (m_statusLeftText && m_statusRightText) {
                switch (st) {
                    case SessionState::Connected:
                        m_statusLeftText->SetLabel(_("Connected"));
                        m_statusLeftText->SetForegroundColour(wxColour(34, 197, 94));
                        break;
                    case SessionState::Connecting:
                        m_statusLeftText->SetLabel(_("Connecting..."));
                        m_statusLeftText->SetForegroundColour(kThemeOrange);
                        break;
                    case SessionState::Reconnecting:
                        m_statusLeftText->SetLabel(_("Reconnecting..."));
                        m_statusLeftText->SetForegroundColour(kThemeOrange);
                        break;
                    default:
                        m_statusLeftText->SetLabel(_("Disconnected"));
                        m_statusLeftText->SetForegroundColour(wxColour(239, 68, 68));
                        break;
                }
                m_statusPanel->Layout();
            }
        }
    }
}

bool MainFrame::IsModified(int index) const {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return false;
    const auto& tab = m_tabs[index];
    if (!tab->activeProfile) return false;
    if (tab->profilePath.IsEmpty()) return true;
    if (!tab->savedProfile) return true;
    return *tab->activeProfile != *tab->savedProfile;
}

wxString MainFrame::GetDisplayName(int index) const {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return _("Untitled");
    const auto& tab = m_tabs[index];
    if (tab->profilePath.IsEmpty()) {
        if (tab->protocol) {
            int count = 0;
            for (int i = 0; i <= index; ++i) {
                if (m_tabs[i]->profilePath.IsEmpty() && m_tabs[i]->protocol == tab->protocol)
                    count++;
            }
            return wxString::Format("Untitled %d - %s", count,
                                    wxString::FromUTF8(tab->protocol->DisplayName()));
        }
        return _("Untitled");
    }
    return wxFileName(tab->profilePath).GetName();
}

void MainFrame::UpdateTabTitle(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    wxString title = GetDisplayName(index);
    if (IsModified(index)) title += " *";

    int welcomeIdx = m_welcomePage ? m_notebook->GetPageIndex(m_welcomePage) : -1;
    int pageIdx = (welcomeIdx != wxNOT_FOUND && index >= welcomeIdx) ? index + 1 : index;

    if (pageIdx >= 0 && pageIdx < (int)m_notebook->GetPageCount() &&
        m_notebook->GetPage(pageIdx) == m_tabs[index]->panel) {
        m_notebook->SetPageText(pageIdx, title);
    }
    if (index == m_activeTabIndex) {
        wxString frameTitle = _("IndComm");
        if (!m_tabs[index]->profilePath.IsEmpty() || m_tabs[index]->activeProfile)
            frameTitle += " - " + title;
        SetTitle(frameTitle);
    }
}

void MainFrame::OnClose(wxCloseEvent& evt) {
    if (m_forceExit) {
        SaveRecentConnections();
        settings::UISettings::Instance().SaveMainWindowGeometry(GetScreenRect(), IsMaximized());
        for (int i = (int)m_tabs.size() - 1; i >= 0; --i) RemoveTab(i);
        if (m_logFrame) {
            m_logFrame->ForceDestroy();
            m_logFrame = nullptr;
        }
        if (m_trayIcon) {
            m_trayIcon->RemoveIcon();
            wxPendingDelete.Append(m_trayIcon);
            m_trayIcon = nullptr;
        }
        Destroy();
        if (wxTheApp) wxTheApp->ExitMainLoop();
        return;
    }
    if (!m_forceExit) {
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i]->activeProfile && IsModified(i)) {
                const int ret = ui::MessageDialog::Show(
                    this, _("Save Connection Profile"),
                    wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName(i)),
                    wxYES_NO | wxCANCEL | wxICON_QUESTION);
                if (ret == wxCANCEL) {
                    evt.Veto();
                    return;
                }
                if (ret == wxYES) {
                    SaveCurrentProfile(false, i);
                    if (IsModified(i)) {
                        evt.Veto();
                        return;
                    }
                }
            }
        }
    }
    if (settings::UISettings::Instance().GetMinimizeToTrayOnClose()) {
        evt.Veto();
        MinimizeToTray();
        return;
    }
    m_forceExit = true;
    Close(true);
}

void MainFrame::OnQuit(wxCommandEvent&) {
    m_forceExit = true;
    Close(true);
}
void MainFrame::OnAbout(wxCommandEvent&) {
    ui::AboutDialog::Show(this, "IndComm", _("A tool for testing industrial protocols."));
}
void MainFrame::OnShowLog(wxCommandEvent&) {
    if (!m_logFrame) m_logFrame = new LogFrame(this);
    m_logFrame->Show();
    m_logFrame->Raise();
}

void MainFrame::OnShowSettings(wxCommandEvent&) {
    SettingsDialog dlg(this);
    dlg.SetApplyCallback([this]() {
        for (auto& tab : m_tabs)
            if (tab->panel) tab->panel->ApplyGlobalSettings();
    });
    dlg.ShowModal();
}

void MainFrame::OnOpenConnection(wxCommandEvent&) {
    const wxString defaultDir = wxStandardPaths::Get().GetDocumentsDir();
    wxFileDialog dlg(this, _("Open Connection Configuration"), defaultDir, "",
                     "Connection files (*.json)|*.json|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    const wxString path = dlg.GetPath();
    ActiveProfile profile;
    if (!ConnectionProfile::Load(path, profile)) {
        ui::MessageDialog::Show(this, _("Error"),
                                wxString::Format(_("Failed to load connection profile:\n%s"), path),
                                wxOK | wxICON_ERROR);
        return;
    }

    AddRecentConnection(path);
    auto* proto = ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) return;
    ISessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;
    AddTab(proto, profile.modeIndex, std::move(session), profile, path);
    m_lastDialogProfile = profile;
}

void MainFrame::OnCloseConnection(wxCommandEvent&) {
    int index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return;
    auto& tab = m_tabs[index];
    if (tab->activeProfile && IsModified(index)) {
        const int ret = ui::MessageDialog::Show(
            this, _("Save Connection Profile"),
            wxString::Format(_("Save changes to '%s' before closing?"), GetDisplayName(index)),
            wxYES_NO | wxCANCEL | wxICON_QUESTION);
        if (ret == wxCANCEL) return;
        if (ret == wxYES) {
            SaveCurrentProfile(false, index);
            if (IsModified(index)) return;
        }
    }
    RemoveTab(index);
}

void MainFrame::OnRecentConnection(wxCommandEvent& evt) {
    size_t index = static_cast<size_t>(evt.GetId() - idMenuRecentBase);
    if (index >= m_recentConnections.GetCount()) return;
    const wxString path = m_recentConnections[index];

    if (!wxFileName::FileExists(path)) {
        ui::MessageDialog::Show(this, _("Error"),
                                wxString::Format(_("Connection profile not found:\n%s"), path),
                                wxOK | wxICON_ERROR);
        m_recentConnections.RemoveAt(index);
        LoadRecentConnections();
        return;
    }
    ActiveProfile profile;
    if (!ConnectionProfile::Load(path, profile)) {
        ui::MessageDialog::Show(this, _("Error"),
                                wxString::Format(_("Failed to load connection profile:\n%s"), path),
                                wxOK | wxICON_ERROR);
        return;
    }
    AddRecentConnection(path);
    auto* proto = ProtocolRegistry::Instance().Find(profile.protocolId);
    if (!proto) return;
    ISessionPtr session = proto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;
    AddTab(proto, profile.modeIndex, std::move(session), profile, path);
    m_lastDialogProfile = profile;
}

void MainFrame::AddRecentConnection(const wxString& path) {
    wxFileName fn(path);
    if (fn.GetExt().Lower() != "json") {
        ICU_SPDLOG_WARN(_("AddRecentConnection: not a .json path, ignored: %s"), path);
        return;
    }
    int idx = m_recentConnections.Index(path);
    if (idx != wxNOT_FOUND) m_recentConnections.RemoveAt(idx);
    m_recentConnections.Insert(path, 0);
    while (m_recentConnections.GetCount() > MAX_RECENT)
        m_recentConnections.RemoveAt(m_recentConnections.GetCount() - 1);
    SaveRecentConnections();
}

void MainFrame::OnLangZhCN(wxCommandEvent&) {
    auto* app = static_cast<icu::App*>(wxTheApp);
    if (app) app->SwitchLanguage("zh_CN");
}
void MainFrame::OnLangEnUS(wxCommandEvent&) {
    auto* app = static_cast<icu::App*>(wxTheApp);
    if (app) app->SwitchLanguage("en_US");
}

void MainFrame::LoadRecentConnections() {
    const wxString packed = settings::UISettings::Instance().GetRecentConnectionsPacked();
    m_recentConnections.Clear();
    if (!packed.IsEmpty()) {
        wxStringTokenizer tok(packed, "\n");
        while (tok.HasMoreTokens()) {
            const wxString path = tok.GetNextToken();
            if (!path.IsEmpty()) m_recentConnections.Add(path);
        }
    }
    for (int i = (int)m_recentConnections.GetCount() - 1; i >= 0; --i) {
        if (!wxFileName::FileExists(m_recentConnections[i])) m_recentConnections.RemoveAt(i);
    }
    SaveRecentConnections();
}

void MainFrame::SaveRecentConnections() {
    wxString packed;
    for (size_t i = 0; i < m_recentConnections.GetCount(); ++i) {
        if (i > 0) packed += "\n";
        packed += m_recentConnections[i];
    }
    settings::UISettings::Instance().SetRecentConnectionsPacked(packed);
}

void MainFrame::SaveCurrentProfile(bool saveAs, int index) {
    if (index == -1) index = m_activeTabIndex;
    if (index < 0 || index >= (int)m_tabs.size()) return;
    auto& tab = m_tabs[index];
    if (!tab->activeProfile) return;
    auto* proto = ProtocolRegistry::Instance().Find(tab->activeProfile->protocolId);
    if (!proto) return;

    if (saveAs || tab->profilePath.IsEmpty()) {
        const wxString defaultName =
            tab->profilePath.IsEmpty()
                ? ConnectionProfile::MakeDefaultFileName(proto, tab->activeProfile->modeIndex,
                                                         tab->activeProfile->fields)
                : wxFileName(tab->profilePath).GetFullName();
        wxString defaultDir = tab->profilePath.IsEmpty() ? wxStandardPaths::Get().GetDocumentsDir()
                                                         : wxFileName(tab->profilePath).GetPath();
        wxFileDialog dlg(this, _("Save Connection Profile"), defaultDir, defaultName,
                         "Connection files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString path = dlg.GetPath();
        if (path.IsEmpty()) return;
        wxFileName fn(path);
        if (fn.GetExt().Lower() != "json") {
            fn.SetExt("json");
            path = fn.GetFullPath();
        }
        tab->profilePath = path;
    }
    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        if (i != index && m_tabs[i]->profilePath == tab->profilePath) {
            ui::MessageDialog::Show(
                this, _("Warning"),
                wxString::Format(_("The file '%s' is already opened in another tab."),
                                 tab->profilePath),
                wxOK | wxICON_WARNING);
            return;
        }
    }
    if (ConnectionProfile::Save(tab->profilePath, tab->activeProfile->protocolId,
                                tab->activeProfile->modeIndex, proto, tab->activeProfile->fields,
                                tab->activeProfile->session)) {
        tab->savedProfile = tab->activeProfile;
        AddRecentConnection(tab->profilePath);
        UpdateTabTitle(index);
        UpdateUIState();
    } else {
        ui::MessageDialog::Show(this, _("Error"),
                                _("Failed to save connection profile.\n\nPlease check if the "
                                  "target folder is writable, or blocked by Windows Defender."),
                                wxOK | wxICON_ERROR);
    }
}

void MainFrame::OnSaveConnection(wxCommandEvent&) {
    SaveCurrentProfile(false);
}
void MainFrame::OnSaveConnectionAs(wxCommandEvent&) {
    SaveCurrentProfile(true);
}
void MainFrame::MinimizeToTray() {
    Hide();
}
void MainFrame::RestoreFromTray() {
    Show();
    Raise();
    Iconize(false);
}

void MainFrame::OnMenuOpenProtocol(wxCommandEvent& evt) {
    const int id = evt.GetId();
    IProtocol* initialProto = nullptr;

    if (id == wxID_NEW || id == idTbNew) {
        initialProto = nullptr;
    } else {
        auto it = m_protocolMenuMap.find(id);
        if (it == m_protocolMenuMap.end()) return;
        initialProto = it->second;
    }

    std::map<std::string, FieldValue> initial;
    int initialMode = 0;
    if (initialProto && m_lastDialogProfile &&
        m_lastDialogProfile->protocolId == initialProto->Id()) {
        initial = m_lastDialogProfile->fields;
        initialMode = m_lastDialogProfile->modeIndex;
    }

    GenericProtocolDialog dlg(this, initialProto, initialMode, initial);
    if (dlg.ShowModal() != wxID_OK) return;

    IProtocol* finalProto = ProtocolRegistry::Instance().Find(dlg.GetProtocolId());
    if (!finalProto) return;

    ActiveProfile profile;
    profile.protocolId = finalProto->Id();
    profile.modeIndex = dlg.GetModeIndex();
    profile.fields = dlg.GetValues();

    m_lastDialogProfile = profile;
    ISessionPtr session = finalProto->CreateSession(profile.modeIndex, profile.fields);
    if (!session) return;

    AddTab(finalProto, profile.modeIndex, std::move(session), profile, wxEmptyString);
}

void MainFrame::OnEditAdd(wxCommandEvent&) {
    auto* tab = GetActiveTab();
    if (tab && tab->panel) tab->panel->CmdAdd();
}
void MainFrame::OnEditCopy(wxCommandEvent&) {
    auto* tab = GetActiveTab();
    if (tab && tab->panel) tab->panel->CmdCopy();
}
void MainFrame::OnEditEdit(wxCommandEvent&) {
    auto* tab = GetActiveTab();
    if (tab && tab->panel) tab->panel->CmdEdit();
}
void MainFrame::OnEditDelete(wxCommandEvent&) {
    auto* tab = GetActiveTab();
    if (tab && tab->panel) tab->panel->CmdDelete();
}

}  // namespace icu
