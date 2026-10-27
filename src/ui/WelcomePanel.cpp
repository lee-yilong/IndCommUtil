#include "WelcomePanel.h"

#include <wx/artprov.h>
#include <wx/bmpbndl.h>
#include <wx/filename.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stdpaths.h>

namespace icu {

namespace {
const wxColour kThemeColor(0, 102, 204);
const wxColour kThemeHoverColor(0, 51, 102);
const wxColour kBgColor(245, 248, 250);
const wxColour kTextColor(50, 50, 50);
const wxColour kSubTextColor(120, 120, 120);

wxBitmapBundle LoadSvgIcon(const wxString& name, int size = 16) {
    wxFileName exePath(wxStandardPaths::Get().GetExecutablePath());
    wxString path = exePath.GetPath() + "/resources/icons/" + name + ".svg";
    if (wxFileExists(path)) {
        wxBitmapBundle bundle = wxBitmapBundle::FromSVGFile(path, wxSize(size, size));
        if (bundle.IsOk()) return bundle;
    }
    wxImage img(size, size);
    img.InitAlpha();
    memset(img.GetAlpha(), 0, size * size);
    return wxBitmapBundle::FromBitmap(wxBitmap(img));
}
}  // namespace

WelcomePanel::WelcomePanel(wxWindow* parent, const wxArrayString& recentFiles)
    : wxPanel(parent, wxID_ANY), m_recentFiles(recentFiles) {
    SetBackgroundColour(kBgColor);
    BuildUi();
}

void WelcomePanel::BuildUi() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 标题
    auto* titleSizer = new wxBoxSizer(wxVERTICAL);
    auto* title = new wxStaticText(this, wxID_ANY, _("IndComm"));
    wxFont titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 12);
    titleFont.MakeBold();
    title->SetFont(titleFont);
    title->SetForegroundColour(kTextColor);
    titleSizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 40);

    auto* subtitle = new wxStaticText(this, wxID_ANY, _("Industrial Communication Utility"));
    wxFont subFont = subtitle->GetFont();
    subFont.SetPointSize(subFont.GetPointSize() + 4);
    subtitle->SetFont(subFont);
    subtitle->SetForegroundColour(kSubTextColor);
    titleSizer->Add(subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 40);

    mainSizer->Add(titleSizer, 0, wxEXPAND);

    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // 左侧
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);
    auto* startLabel = new wxStaticText(this, wxID_ANY, _("Start"));
    wxFont secFont = startLabel->GetFont();
    secFont.SetPointSize(secFont.GetPointSize() + 1);
    secFont.MakeBold();
    startLabel->SetFont(secFont);
    startLabel->SetForegroundColour(kTextColor);
    leftSizer->Add(startLabel, 0, wxBOTTOM, 10);

    auto addLink = [&](const wxString& text, std::function<void()> onClick) {
        auto* link = new wxStaticText(this, wxID_ANY, text);
        link->SetForegroundColour(kThemeColor);
        link->SetCursor(wxCursor(wxCURSOR_HAND));
        link->Bind(wxEVT_ENTER_WINDOW, [link](wxMouseEvent& e) {
            link->SetForegroundColour(kThemeHoverColor);
            link->Refresh();
            e.Skip();
        });
        link->Bind(wxEVT_LEAVE_WINDOW, [link](wxMouseEvent& e) {
            link->SetForegroundColour(kThemeColor);
            link->Refresh();
            e.Skip();
        });
        link->Bind(wxEVT_LEFT_UP, [onClick](wxMouseEvent&) {
            if (onClick) onClick();
        });
        leftSizer->Add(link, 0, wxBOTTOM, 12);
    };

    addLink(_("New Connection..."), [this]() {
        if (m_onNewClick) m_onNewClick();
    });
    addLink(_("Open Connection..."), [this]() {
        if (m_onOpenClick) m_onOpenClick();
    });

    // 右侧
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);
    auto* recentLabel = new wxStaticText(this, wxID_ANY, _("Recent"));
    recentLabel->SetFont(secFont);
    recentLabel->SetForegroundColour(kTextColor);
    rightSizer->Add(recentLabel, 0, wxBOTTOM, 10);

    if (m_recentFiles.IsEmpty()) {
        auto* emptyText = new wxStaticText(this, wxID_ANY, _("No recent connections."));
        emptyText->SetForegroundColour(kSubTextColor);
        rightSizer->Add(emptyText, 0, wxALL, 5);
    } else {
        for (size_t i = 0; i < m_recentFiles.GetCount(); ++i) {
            wxFileName fn(m_recentFiles[i]);
            auto* link = new wxStaticText(this, wxID_ANY, fn.GetFullName());
            link->SetForegroundColour(kThemeColor);
            link->SetCursor(wxCursor(wxCURSOR_HAND));
            link->Bind(wxEVT_ENTER_WINDOW, [link](wxMouseEvent& e) {
                link->SetForegroundColour(kThemeHoverColor);
                link->Refresh();
                e.Skip();
            });
            link->Bind(wxEVT_LEAVE_WINDOW, [link](wxMouseEvent& e) {
                link->SetForegroundColour(kThemeColor);
                link->Refresh();
                e.Skip();
            });
            link->Bind(wxEVT_LEFT_UP, [this, i](wxMouseEvent&) {
                if (m_onRecentClick) m_onRecentClick(static_cast<int>(i));
            });
            rightSizer->Add(link, 0, wxBOTTOM | wxTOP, 6);
        }
    }

    contentSizer->Add(leftSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 40);
    contentSizer->Add(rightSizer, 1, wxEXPAND | wxRIGHT, 40);

    mainSizer->Add(contentSizer, 1, wxEXPAND);
    SetSizer(mainSizer);
}

}  // namespace icu
