#include "GenericProtocolDialog.h"

#include <wx/artprov.h>
#include <wx/dcbuffer.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stdpaths.h>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

#include "core/ProtocolRegistry.h"

namespace icu {

namespace {
const wxColour kThemeBlue(56, 189, 248);
const wxColour kThemeDark(30, 41, 59);
const wxColour kBgColor(243, 244, 246);
const wxColour kCardBgColor(255, 255, 255);
const wxColour kTextColor(51, 65, 85);
const wxColour kSubTextColor(100, 116, 139);
const wxColour kBorderColor(209, 213, 219);
const wxColour kSelectBgColor(239, 246, 255);
const wxColour kBtnGrayBg(229, 231, 235);
const wxColour kCloseHoverColor(239, 68, 68);
const wxColour kInputBgColor(248, 250, 252);  // 柔和的输入框背景色

wxString SafeTrans(const wxString& text) {
    return text.IsEmpty() ? " " : text;
}

wxBitmapBundle LoadSvgIcon(const wxString& name, int size = 16, wxColour tint = wxNullColour) {
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

// 去除原生控件下沉边框的辅助函数
void RemoveNativeBorder(wxWindow* ctrl) {
#ifdef __WXMSW__
    HWND hwnd = (HWND)ctrl->GetHandle();
    if (hwnd) {
        LONG_PTR exStyle = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        ::SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_CLIENTEDGE);
    }
#endif
}

class CardPanel : public wxPanel {
public:
    CardPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(kCardBgColor);
    }

protected:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetBrush(wxBrush(kCardBgColor));
            gc->SetPen(wxPen(kBorderColor, 1));
            gc->DrawRoundedRectangle(0.5, 0.5, GetSize().x - 1, GetSize().y - 1, 8);
        }
    }

private:
    wxDECLARE_EVENT_TABLE();
};
wxBEGIN_EVENT_TABLE(CardPanel, wxPanel) EVT_PAINT(CardPanel::OnPaint) wxEND_EVENT_TABLE()

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
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) {
            if (m_onClick) m_onClick();
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

protected:
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

private:
    wxString m_text;
    wxColour m_bg, m_fg;
    bool m_hover = false;
    bool m_enabled = true;
    std::function<void()> m_onClick;
    wxDECLARE_EVENT_TABLE();
};
wxBEGIN_EVENT_TABLE(FlatButton, wxPanel) EVT_PAINT(FlatButton::OnPaint) wxEND_EVENT_TABLE()

}  // namespace

wxBEGIN_EVENT_TABLE(StepIndicatorPanel, wxPanel) EVT_PAINT(StepIndicatorPanel::OnPaint)
    wxEND_EVENT_TABLE()

        StepIndicatorPanel::StepIndicatorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 60)) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void StepIndicatorPanel::SetCurrentStep(int currentStep) {
    m_currentStep = currentStep;
    Refresh();
}

void StepIndicatorPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
    dc.Clear();

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    wxSize size = GetClientSize();
    int marginX = 40;
    int usableWidth = size.x - marginX * 2;
    int stepSpacing = usableWidth / (kTotalSteps - 1);
    int cy = size.y / 2 - 8;
    int radius = 12;

    const wxString stepNames[] = {_("Protocol"), _("Mode"), _("Role"), _("Parameters"),
                                  _("Finish")};

    // ★ 修改点：将线条颜色改为背景色，视觉上移除未完成线条
    gc->SetPen(wxPen(kBgColor, 2));
    gc->StrokeLine(marginX, cy, marginX + usableWidth, cy);

    if (m_currentStep > 0) {
        gc->SetPen(wxPen(kThemeBlue, 2));
        gc->StrokeLine(marginX, cy, marginX + stepSpacing * m_currentStep, cy);
    }

    for (int i = 0; i < kTotalSteps; ++i) {
        int cx = marginX + stepSpacing * i;
        bool completed = (i < m_currentStep);
        bool current = (i == m_currentStep);

        wxColour circleColor = completed ? kThemeBlue : (current ? kTextColor : kBorderColor);
        wxColour textColor = (completed || current) ? *wxWHITE : kSubTextColor;

        gc->SetBrush(wxBrush(circleColor));
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->DrawEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

        wxFont numFont = GetFont();
        numFont.SetPointSize(9);
        numFont.MakeBold();
        gc->SetFont(numFont, textColor);
        wxString numStr = wxString::Format("%d", i + 1);
        double tw, th;
        gc->GetTextExtent(numStr, &tw, &th);
        gc->DrawText(numStr, cx - tw / 2, cy - th / 2);

        wxFont labelFont = GetFont();
        labelFont.SetPointSize(8);
        if (current) labelFont.MakeBold();

        wxColour labelColor = completed ? kThemeBlue : (current ? kThemeDark : kSubTextColor);
        gc->SetFont(labelFont, labelColor);
        double lw, lh;
        gc->GetTextExtent(stepNames[i], &lw, &lh);
        gc->DrawText(stepNames[i], cx - lw / 2, cy + radius + 6);
    }
}

wxBEGIN_EVENT_TABLE(GenericProtocolDialog, wxDialog) EVT_PAINT(GenericProtocolDialog::OnPaint)
    EVT_LEFT_DOWN(GenericProtocolDialog::OnMouseDown) EVT_MOTION(GenericProtocolDialog::OnMouseMove)
        EVT_LEFT_UP(GenericProtocolDialog::OnMouseUp) wxEND_EVENT_TABLE()

            GenericProtocolDialog::GenericProtocolDialog(
                wxWindow* parent, IProtocol* protocol, int initialModeIndex,
                const std::map<std::string, FieldValue>& initialValues)
    : wxDialog(parent, wxID_ANY, _("New Connection Wizard"), wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE | wxRESIZE_BORDER),
      m_protocol(protocol),
      m_values(initialValues),
      m_selectedModeIndex(initialModeIndex) {
    if (!m_protocol) {
        auto all = ProtocolRegistry::Instance().All();
        if (!all.empty()) m_protocol = all[0];
    }
    BuildUi();
    BuildPages();
    UpdateButtonsState();
}

void GenericProtocolDialog::BuildUi() {
    SetBackgroundColour(kBgColor);
    SetDoubleBuffered(true);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* titlePanel = new wxPanel(this);
    titlePanel->SetBackgroundColour(kBgColor);
    auto* titleSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* titleText = new wxStaticText(titlePanel, wxID_ANY, _("New Connection"));
    wxFont titleFont = titleText->GetFont();
    titleFont.SetPointSize(12);
    titleFont.MakeBold();
    titleText->SetFont(titleFont);
    titleText->SetForegroundColour(kThemeDark);

    auto* closeBtn =
        new wxStaticBitmap(titlePanel, wxID_ANY, LoadSvgIcon("close", 16, kSubTextColor));
    closeBtn->SetCursor(wxCursor(wxCURSOR_HAND));
    closeBtn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });
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
    mainSizer->Add(titlePanel, 0, wxEXPAND | wxTOP | wxBOTTOM, 12);

    titlePanel->Bind(wxEVT_LEFT_DOWN, &GenericProtocolDialog::OnMouseDown, this);
    titlePanel->Bind(wxEVT_MOTION, &GenericProtocolDialog::OnMouseMove, this);
    titlePanel->Bind(wxEVT_LEFT_UP, &GenericProtocolDialog::OnMouseUp, this);
    titleText->Bind(wxEVT_LEFT_DOWN, &GenericProtocolDialog::OnMouseDown, this);
    titleText->Bind(wxEVT_MOTION, &GenericProtocolDialog::OnMouseMove, this);
    titleText->Bind(wxEVT_LEFT_UP, &GenericProtocolDialog::OnMouseUp, this);

    m_indicator = new StepIndicatorPanel(this);
    mainSizer->Add(m_indicator, 0, wxEXPAND | wxBOTTOM, 10);

    m_book = new wxSimplebook(this);
    m_book->SetBackgroundColour(kBgColor);
    mainSizer->Add(m_book, 1, wxEXPAND | wxLEFT | wxRIGHT, 20);

    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnCancel = new FlatButton(this, _("Cancel"), kBtnGrayBg, kTextColor,
                                     [this]() { EndModal(wxID_CANCEL); });
    auto* btnBack = new FlatButton(this, _("Back"), kBtnGrayBg, kTextColor, [this]() {
        wxCommandEvent e;
        OnBack(e);
    });
    auto* btnNext = new FlatButton(this, _("Next"), kThemeDark, *wxWHITE, [this]() {
        wxCommandEvent e;
        OnNext(e);
    });
    auto* btnFinish =
        new FlatButton(this, _("Finish"), kThemeDark, *wxWHITE, [this]() { EndModal(wxID_OK); });

    m_btnBack = reinterpret_cast<wxButton*>(btnBack);
    m_btnNext = reinterpret_cast<wxButton*>(btnNext);
    m_btnFinish = reinterpret_cast<wxButton*>(btnFinish);

    btnSizer->Add(btnCancel, 0, wxRIGHT, 10);
    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnBack, 0, wxRIGHT, 10);
    btnSizer->Add(btnNext, 0, wxRIGHT, 10);
    btnSizer->Add(btnFinish, 0);

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 16);
    SetSizer(mainSizer);
    SetSize(FromDIP(wxSize(650, 500)));
    SetMinSize(FromDIP(wxSize(650, 500)));
    CentreOnParent();
}

void GenericProtocolDialog::BuildPages() {
    m_book->DeleteAllPages();
    m_book->AddPage(CreateProtocolPage(m_book), _("Protocol"));
    m_book->AddPage(CreateModePage(m_book), _("Mode"));
    m_book->AddPage(CreateRolePage(m_book), _("Role"));
    m_book->AddPage(CreateParamsPage(m_book), _("Params"));
    m_book->AddPage(CreateFinishPage(m_book), _("Finish"));

    m_currentStep = 0;
    m_indicator->SetCurrentStep(m_currentStep);
    m_book->SetSelection(m_currentStep);
}

void GenericProtocolDialog::RebuildPages() {
    SaveCurrentPageData();
}

wxPanel* GenericProtocolDialog::CreateProtocolPage(wxWindow* parent) {
    auto* p = new wxPanel(parent);
    p->SetBackgroundColour(kBgColor);
    auto* mainSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftCard = new CardPanel(p);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);
    leftCard->SetSizer(leftSizer);

    auto* listTitle = new wxStaticText(leftCard, wxID_ANY, _("Select Protocol"));
    wxFont lf = listTitle->GetFont();
    lf.MakeBold();
    listTitle->SetFont(lf);
    listTitle->SetForegroundColour(kTextColor);
    leftSizer->Add(listTitle, 0, wxALL, 16);

    auto* listScroll =
        new wxScrolledWindow(leftCard, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    listScroll->SetScrollRate(0, 10);
    listScroll->SetBackgroundColour(kCardBgColor);
    auto* listSizer = new wxBoxSizer(wxVERTICAL);

    auto allProtos = ProtocolRegistry::Instance().All();
    for (auto* proto : allProtos) {
        bool isSel = (m_protocol && proto->Id() == m_protocol->Id());

        auto* item =
            new wxPanel(listScroll, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        item->SetBackgroundColour(isSel ? kSelectBgColor : kCardBgColor);
        auto* itemSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* icon = new wxStaticBitmap(item, wxID_ANY, LoadSvgIcon("connect", 24, kTextColor));
        itemSizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);

        auto* name = new wxStaticText(item, wxID_ANY, wxString::FromUTF8(proto->DisplayName()));
        wxFont nf = name->GetFont();
        nf.SetPointSize(10);
        if (isSel) nf.MakeBold();
        name->SetFont(nf);
        name->SetForegroundColour(isSel ? kThemeDark : kTextColor);
        itemSizer->Add(name, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);

        auto* rb =
            new wxRadioButton(item, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
        rb->SetValue(isSel);
        itemSizer->Add(rb, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

        item->SetSizer(itemSizer);
        item->SetMinSize(wxSize(-1, 40));
        listSizer->Add(item, 0, wxEXPAND | wxBOTTOM, 2);

        auto updateSelection = [this, proto, item, icon, name, rb, listScroll]() {
            if (m_protocol != proto) {
                m_protocol = proto;
                m_values.clear();
                m_selectedModeIndex = 0;

                for (auto* child : listScroll->GetChildren()) {
                    if (auto* pnl = dynamic_cast<wxPanel*>(child)) {
                        pnl->SetBackgroundColour(kCardBgColor);
                        for (auto* sub : pnl->GetChildren()) {
                            if (auto* lbl = dynamic_cast<wxStaticText*>(sub)) {
                                lbl->SetForegroundColour(kTextColor);
                                wxFont f = lbl->GetFont();
                                f.SetWeight(wxFONTWEIGHT_NORMAL);
                                lbl->SetFont(f);
                            }
                            if (auto* r = dynamic_cast<wxRadioButton*>(sub)) r->SetValue(false);
                        }
                        pnl->Refresh();
                    }
                }
                item->SetBackgroundColour(kSelectBgColor);
                name->SetForegroundColour(kThemeDark);
                wxFont f = name->GetFont();
                f.SetWeight(wxFONTWEIGHT_BOLD);
                name->SetFont(f);
                rb->SetValue(true);
                item->Refresh();
                RefreshProtocolDetail();
            }
        };

        auto onClick = [updateSelection](wxMouseEvent& evt) {
            updateSelection();
            evt.Skip();
        };
        item->Bind(wxEVT_LEFT_UP, onClick);
        icon->Bind(wxEVT_LEFT_UP, onClick);
        name->Bind(wxEVT_LEFT_UP, onClick);
        rb->Bind(wxEVT_RADIOBUTTON, [updateSelection](wxCommandEvent& evt) {
            updateSelection();
            evt.Skip();
        });
    }

    listScroll->SetSizer(listSizer);
    leftSizer->Add(listScroll, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    mainSizer->Add(leftCard, 1, wxEXPAND | wxRIGHT, 15);

    m_protoDetailPanel = new CardPanel(p);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);
    m_protoDetailPanel->SetSizer(rightSizer);
    mainSizer->Add(m_protoDetailPanel, 2, wxEXPAND);

    p->SetSizer(mainSizer);
    RefreshProtocolDetail();
    return p;
}

void GenericProtocolDialog::RefreshProtocolDetail() {
    if (!m_protoDetailPanel || !m_protocol) return;

    auto* sizer = m_protoDetailPanel->GetSizer();
    sizer->Clear(true);

    auto* iconRow = new wxBoxSizer(wxHORIZONTAL);
    auto* icon =
        new wxStaticBitmap(m_protoDetailPanel, wxID_ANY, LoadSvgIcon("open", 48, kTextColor));
    iconRow->Add(icon, 0, wxALL, 20);

    auto* textCol = new wxBoxSizer(wxVERTICAL);
    auto* nameText = new wxStaticText(m_protoDetailPanel, wxID_ANY,
                                      wxString::FromUTF8(m_protocol->DisplayName()));
    wxFont nf = nameText->GetFont();
    nf.SetPointSize(16);
    nf.MakeBold();
    nameText->SetFont(nf);
    nameText->SetForegroundColour(kThemeDark);
    textCol->Add(nameText, 0, wxTOP, 20);

    auto* descText = new wxStaticText(m_protoDetailPanel, wxID_ANY,
                                      _("Based on this protocol, client/server communication."));
    descText->SetForegroundColour(kSubTextColor);
    textCol->Add(descText, 0, wxTOP, 8);

    iconRow->Add(textCol, 1, wxEXPAND);
    sizer->Add(iconRow, 0, wxEXPAND);

    auto* infoCard =
        new wxPanel(m_protoDetailPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    infoCard->SetBackgroundColour(wxColour(249, 250, 251));
    auto* infoSizer = new wxBoxSizer(wxVERTICAL);

    auto* infoTitle = new wxStaticText(infoCard, wxID_ANY, _("Protocol Details"));
    wxFont itf = infoTitle->GetFont();
    itf.MakeBold();
    infoTitle->SetFont(itf);
    infoTitle->SetForegroundColour(kTextColor);
    infoSizer->Add(infoTitle, 0, wxALL, 12);

    auto* infoGrid = new wxFlexGridSizer(2, 8, 16);
    auto* lblType = new wxStaticText(infoCard, wxID_ANY, _("Type:"));
    lblType->SetForegroundColour(kSubTextColor);
    infoGrid->Add(lblType, 0);

    auto* valType =
        new wxStaticText(infoCard, wxID_ANY, wxString::FromUTF8(m_protocol->DisplayName()));
    valType->SetForegroundColour(kTextColor);
    infoGrid->Add(valType, 0);

    auto* lblFeat = new wxStaticText(infoCard, wxID_ANY, _("Features:"));
    lblFeat->SetForegroundColour(kSubTextColor);
    infoGrid->Add(lblFeat, 0);

    auto* valFeat = new wxStaticText(infoCard, wxID_ANY,
                                     _("Reliable, stable, suitable for industrial control."));
    valFeat->SetForegroundColour(kTextColor);
    infoGrid->Add(valFeat, 0);

    infoSizer->Add(infoGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    infoCard->SetSizer(infoSizer);

    sizer->Add(infoCard, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    m_protoDetailPanel->Layout();
}

wxPanel* GenericProtocolDialog::CreateModePage(wxWindow* parent) {
    auto* p = new wxPanel(parent);
    p->SetBackgroundColour(kBgColor);

    auto* card = new CardPanel(p);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    card->SetSizer(cardSizer);

    auto* title = new wxStaticText(card, wxID_ANY, _("Select Mode"));
    wxFont tf = title->GetFont();
    tf.SetPointSize(12);
    tf.MakeBold();
    title->SetFont(tf);
    title->SetForegroundColour(kThemeDark);
    cardSizer->Add(title, 0, wxALL, 16);

    m_modeContentPanel =
        new wxPanel(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_modeContentPanel->SetBackgroundColour(kCardBgColor);
    cardSizer->Add(m_modeContentPanel, 1, wxEXPAND);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(card, 1, wxEXPAND);
    p->SetSizer(mainSizer);
    return p;
}

void GenericProtocolDialog::RefreshModePage() {
    if (!m_modeContentPanel) return;
    m_modeContentPanel->DestroyChildren();
    auto* s = new wxBoxSizer(wxVERTICAL);
    if (m_protocol && m_protocol->HasModes()) {
        auto names = m_protocol->ModeNames();
        for (size_t i = 0; i < names.size(); ++i) {
            auto* rb = new wxRadioButton(m_modeContentPanel, wxID_ANY,
                                         wxGetTranslation(wxString::FromUTF8(names[i])),
                                         wxDefaultPosition, wxDefaultSize, i == 0 ? wxRB_GROUP : 0);
            if ((int)i == m_selectedModeIndex) rb->SetValue(true);
            rb->SetForegroundColour(kTextColor);
            rb->Bind(wxEVT_RADIOBUTTON, [this, i](wxCommandEvent& evt) {
                m_selectedModeIndex = (int)i;
                evt.Skip();
            });
            s->Add(rb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
        }
    } else {
        auto* emptyText = new wxStaticText(m_modeContentPanel, wxID_ANY,
                                           _("This protocol does not have multiple modes."));
        emptyText->SetForegroundColour(kSubTextColor);
        s->Add(emptyText, 1, wxALIGN_CENTER_HORIZONTAL | wxTOP, 40);
    }
    m_modeContentPanel->SetSizer(s);
    m_modeContentPanel->Layout();
}

wxPanel* GenericProtocolDialog::CreateRolePage(wxWindow* parent) {
    auto* p = new wxPanel(parent);
    p->SetBackgroundColour(kBgColor);

    auto* card = new CardPanel(p);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    card->SetSizer(cardSizer);

    auto* title = new wxStaticText(card, wxID_ANY, _("Select Role"));
    wxFont tf = title->GetFont();
    tf.SetPointSize(12);
    tf.MakeBold();
    title->SetFont(tf);
    title->SetForegroundColour(kThemeDark);
    cardSizer->Add(title, 0, wxALL, 16);

    m_roleContentPanel =
        new wxPanel(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_roleContentPanel->SetBackgroundColour(kCardBgColor);
    cardSizer->Add(m_roleContentPanel, 1, wxEXPAND);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(card, 1, wxEXPAND);
    p->SetSizer(mainSizer);
    return p;
}

void GenericProtocolDialog::RefreshRolePage() {
    if (!m_roleContentPanel) return;
    m_roleContentPanel->DestroyChildren();
    auto* s = new wxBoxSizer(wxVERTICAL);
    bool hasRole = false;
    if (m_protocol) {
        for (const auto& fd : m_protocol->FieldsForMode(m_selectedModeIndex)) {
            if (fd.key == "role") {
                hasRole = true;
                for (size_t i = 0; i < fd.choices.size(); ++i) {
                    auto* rb = new wxRadioButton(
                        m_roleContentPanel, wxID_ANY,
                        wxGetTranslation(wxString::FromUTF8(fd.choices[i].second)),
                        wxDefaultPosition, wxDefaultSize, i == 0 ? wxRB_GROUP : 0);
                    std::string curRole = m_values.count("role") ? m_values["role"].AsString()
                                                                 : fd.defaultValue.AsString();
                    if (fd.choices[i].first == curRole) rb->SetValue(true);
                    rb->SetForegroundColour(kTextColor);
                    rb->Bind(wxEVT_RADIOBUTTON, [this, fd, i](wxCommandEvent& evt) {
                        m_values["role"] = fd.choices[i].first;
                        evt.Skip();
                    });
                    s->Add(rb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
                }
                break;
            }
        }
    }
    if (!hasRole) {
        auto* emptyText = new wxStaticText(m_roleContentPanel, wxID_ANY,
                                           _("This protocol does not require a role selection."));
        emptyText->SetForegroundColour(kSubTextColor);
        s->Add(emptyText, 1, wxALIGN_CENTER_HORIZONTAL | wxTOP, 40);
    }
    m_roleContentPanel->SetSizer(s);
    m_roleContentPanel->Layout();
}

wxPanel* GenericProtocolDialog::CreateParamsPage(wxWindow* parent) {
    auto* p = new wxPanel(parent);
    p->SetBackgroundColour(kBgColor);

    auto* card = new CardPanel(p);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    card->SetSizer(cardSizer);

    auto* title = new wxStaticText(card, wxID_ANY, _("Set Parameters"));
    wxFont tf = title->GetFont();
    tf.SetPointSize(12);
    tf.MakeBold();
    title->SetFont(tf);
    title->SetForegroundColour(kThemeDark);
    cardSizer->Add(title, 0, wxALL, 16);

    m_paramContentPanel =
        new wxPanel(card, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_paramContentPanel->SetBackgroundColour(kCardBgColor);
    cardSizer->Add(m_paramContentPanel, 1, wxEXPAND);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(card, 1, wxEXPAND);
    p->SetSizer(mainSizer);
    return p;
}

void GenericProtocolDialog::RefreshParamsPage() {
    if (!m_paramContentPanel || !m_protocol) return;
    m_paramContentPanel->DestroyChildren();
    auto* s = new wxBoxSizer(wxVERTICAL);
    auto* scroll = new wxScrolledWindow(m_paramContentPanel, wxID_ANY);
    scroll->SetBackgroundColour(kCardBgColor);
    scroll->SetScrollRate(0, 10);
    auto* gridSizer = new wxFlexGridSizer(2, 12, 12);
    m_controlMap.clear();
    std::string currentRole = m_values.count("role") ? m_values["role"].AsString() : "";
    auto fields = m_protocol->FieldsForMode(m_selectedModeIndex);
    for (const auto& fd : fields) {
        if (fd.key == "role") continue;
        if (fd.key == "maxClients" && currentRole != "Server") continue;

        auto* lbl =
            new wxStaticText(scroll, wxID_ANY, wxGetTranslation(wxString::FromUTF8(fd.label)));
        lbl->SetForegroundColour(kTextColor);
        gridSizer->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);

        wxWindow* ctrl = CreateControl(scroll, fd);
        gridSizer->Add(ctrl, 1, wxEXPAND);
        m_controlMap[fd.key] = ctrl;

        if (m_values.count(fd.key)) {
            const FieldValue& v = m_values[fd.key];
            if (fd.type == FieldValue::Type::String)
                static_cast<wxTextCtrl*>(ctrl)->SetValue(wxString::FromUTF8(v.AsString()));
            else if (fd.type == FieldValue::Type::Int)
                static_cast<wxSpinCtrl*>(ctrl)->SetValue(v.AsInt());
            else if (fd.type == FieldValue::Type::Bool)
                static_cast<wxCheckBox*>(ctrl)->SetValue(v.AsBool());
            else if (fd.type == FieldValue::Type::Choice) {
                auto* ch = static_cast<wxChoice*>(ctrl);
                for (size_t i = 0; i < fd.choices.size(); ++i) {
                    if (fd.choices[i].first == v.AsString()) {
                        ch->SetSelection(i);
                        break;
                    }
                }
            }
        }
    }
    scroll->SetSizer(gridSizer);
    s->Add(scroll, 1, wxEXPAND | wxALL, 12);
    m_paramContentPanel->SetSizer(s);
    m_paramContentPanel->Layout();
}

wxPanel* GenericProtocolDialog::CreateFinishPage(wxWindow* parent) {
    auto* p = new wxPanel(parent);
    p->SetBackgroundColour(kBgColor);

    auto* card = new CardPanel(p);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    card->SetSizer(cardSizer);

    auto* title = new wxStaticText(card, wxID_ANY, _("Setup Complete"));
    wxFont tf = title->GetFont();
    tf.SetPointSize(16);
    tf.MakeBold();
    title->SetFont(tf);
    title->SetForegroundColour(kThemeBlue);

    cardSizer->AddStretchSpacer(1);
    cardSizer->Add(title, 0, wxALIGN_CENTER_HORIZONTAL);
    auto* sub = new wxStaticText(card, wxID_ANY, _("Click Finish to create the connection."));
    sub->SetForegroundColour(kSubTextColor);
    cardSizer->Add(sub, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, 8);
    cardSizer->AddStretchSpacer(1);

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(card, 1, wxEXPAND);
    p->SetSizer(mainSizer);
    return p;
}

wxWindow* GenericProtocolDialog::CreateControl(wxWindow* parent, const FieldDescriptor& fd) {
    const wxSize ctrlSize = FromDIP(wxSize(220, 32));  // 稍微加高一点，更现代
    wxWindow* ctrl = nullptr;

    switch (fd.type) {
        case FieldValue::Type::String:
            ctrl = new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(fd.defaultValue.AsString()),
                                  wxDefaultPosition, ctrlSize, wxBORDER_NONE);
            break;
        case FieldValue::Type::Int:
            ctrl = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, ctrlSize,
                                  wxSP_ARROW_KEYS | wxBORDER_NONE, fd.minValue, fd.maxValue,
                                  fd.defaultValue.AsInt());
            break;
        case FieldValue::Type::Bool: {
            auto* c = new wxCheckBox(parent, wxID_ANY, "");
            c->SetValue(fd.defaultValue.AsBool());
            ctrl = c;
            break;
        }
        case FieldValue::Type::Choice: {
            wxArrayString labels;
            int sel = 0;
            for (size_t i = 0; i < fd.choices.size(); ++i) {
                labels.Add(wxGetTranslation(wxString::FromUTF8(fd.choices[i].second)));
                if (fd.choices[i].first == fd.defaultValue.AsString()) sel = (int)i;
            }
            // ★ 修复：移除了多余的 0，直接传入 wxBORDER_NONE
            auto* ch =
                new wxChoice(parent, wxID_ANY, wxDefaultPosition, ctrlSize, labels, wxBORDER_NONE);
            ch->SetSelection(sel);
            ctrl = ch;
            break;
        }
        default:
            ctrl = new wxTextCtrl(parent, wxID_ANY, "", wxDefaultPosition, ctrlSize, wxBORDER_NONE);
            break;
    }

    if (ctrl) {
        ctrl->SetBackgroundColour(kInputBgColor);
        // 强行干掉 Windows 原生下沉 3D 边框
        RemoveNativeBorder(ctrl);
    }
    return ctrl;
}

void GenericProtocolDialog::SaveCurrentPageData() {
    if (m_currentStep == 3 && m_paramContentPanel) {
        for (const auto& fd : m_protocol->FieldsForMode(m_selectedModeIndex)) {
            if (fd.key == "role") continue;
            if (m_controlMap.count(fd.key)) {
                wxWindow* w = m_controlMap[fd.key];
                if (fd.type == FieldValue::Type::String)
                    m_values[fd.key] =
                        std::string(static_cast<wxTextCtrl*>(w)->GetValue().utf8_str());
                else if (fd.type == FieldValue::Type::Int)
                    m_values[fd.key] = static_cast<wxSpinCtrl*>(w)->GetValue();
                else if (fd.type == FieldValue::Type::Bool)
                    m_values[fd.key] = static_cast<wxCheckBox*>(w)->GetValue();
                else if (fd.type == FieldValue::Type::Choice) {
                    auto* ch = static_cast<wxChoice*>(w);
                    int idx = ch->GetSelection();
                    if (idx >= 0 && idx < (int)fd.choices.size())
                        m_values[fd.key] = fd.choices[idx].first;
                }
            }
        }
    }
}

void GenericProtocolDialog::UpdateButtonsState() {
    FlatButton* backBtn = reinterpret_cast<FlatButton*>(m_btnBack);
    FlatButton* nextBtn = reinterpret_cast<FlatButton*>(m_btnNext);
    FlatButton* finishBtn = reinterpret_cast<FlatButton*>(m_btnFinish);

    if (backBtn) backBtn->SetBtnEnabled(m_currentStep > 0);

    if (m_currentStep == 4) {
        if (nextBtn) nextBtn->Hide();
        if (finishBtn) finishBtn->Show();
    } else {
        if (nextBtn) nextBtn->Show();
        if (finishBtn) finishBtn->Hide();
    }
    Layout();
}

void GenericProtocolDialog::OnNext(wxCommandEvent&) {
    if (m_currentStep < 4) {
        SaveCurrentPageData();
        m_currentStep++;
        m_indicator->SetCurrentStep(m_currentStep);
        if (m_currentStep == 1)
            RefreshModePage();
        else if (m_currentStep == 2)
            RefreshRolePage();
        else if (m_currentStep == 3)
            RefreshParamsPage();
        m_book->SetSelection(m_currentStep);
        UpdateButtonsState();
    }
}

void GenericProtocolDialog::OnBack(wxCommandEvent&) {
    if (m_currentStep > 0) {
        SaveCurrentPageData();
        m_currentStep--;
        m_indicator->SetCurrentStep(m_currentStep);
        m_book->SetSelection(m_currentStep);
        UpdateButtonsState();
    }
}

int GenericProtocolDialog::GetModeIndex() const {
    return m_selectedModeIndex;
}
std::string GenericProtocolDialog::GetProtocolId() const {
    return m_protocol ? m_protocol->Id() : "";
}
std::map<std::string, FieldValue> GenericProtocolDialog::GetValues() const {
    return m_values;
}

void GenericProtocolDialog::OnPaint(wxPaintEvent& evt) {
    wxPaintDC dc(this);
    evt.Skip();
}

void GenericProtocolDialog::OnMouseDown(wxMouseEvent& evt) {
    if (evt.LeftIsDown()) {
        m_dragStart = wxGetMousePosition();
        m_winStart = GetPosition();
        if (!HasCapture()) CaptureMouse();
    }
    evt.Skip();
}

void GenericProtocolDialog::OnMouseMove(wxMouseEvent& evt) {
    if (evt.LeftIsDown() && HasCapture()) {
        wxPoint delta = wxGetMousePosition() - m_dragStart;
        Move(m_winStart + delta);
    }
    evt.Skip();
}

void GenericProtocolDialog::OnMouseUp(wxMouseEvent& evt) {
    if (HasCapture()) ReleaseMouse();
    evt.Skip();
}

}  // namespace icu
