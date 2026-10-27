#pragma once

#include <wx/listbox.h>
#include <wx/radiobut.h>
#include <wx/simplebook.h>
#include <wx/wx.h>

#include <map>
#include <vector>

#include "core/IProtocol.h"

namespace icu {

class StepIndicatorPanel : public wxPanel {
public:
    explicit StepIndicatorPanel(wxWindow* parent);
    void SetCurrentStep(int currentStep);
    static constexpr int kTotalSteps = 5;

private:
    int m_currentStep = 0;
    void OnPaint(wxPaintEvent& evt);
    wxDECLARE_EVENT_TABLE();
};

class GenericProtocolDialog : public wxDialog {
public:
    GenericProtocolDialog(wxWindow* parent, IProtocol* protocol, int initialModeIndex,
                          const std::map<std::string, FieldValue>& initialValues);

    std::string GetProtocolId() const;
    int GetModeIndex() const;
    std::map<std::string, FieldValue> GetValues() const;

private:
    IProtocol* m_protocol = nullptr;
    std::map<std::string, FieldValue> m_values;
    int m_selectedModeIndex = 0;

    int m_currentStep = 0;

    StepIndicatorPanel* m_indicator = nullptr;
    wxSimplebook* m_book = nullptr;

    wxButton* m_btnBack = nullptr;
    wxButton* m_btnNext = nullptr;
    wxButton* m_btnFinish = nullptr;

    wxListBox* m_protoListBox = nullptr;
    wxPanel* m_protoDetailPanel = nullptr;

    wxPanel* m_modeContentPanel = nullptr;
    wxPanel* m_roleContentPanel = nullptr;
    wxPanel* m_paramContentPanel = nullptr;

    std::map<std::string, wxWindow*> m_controlMap;

    void BuildUi();
    void BuildPages();
    void RebuildPages();  // ★ 核心修复：补充缺失的声明

    wxPanel* CreateProtocolPage(wxWindow* parent);
    wxPanel* CreateModePage(wxWindow* parent);
    wxPanel* CreateRolePage(wxWindow* parent);
    wxPanel* CreateParamsPage(wxWindow* parent);
    wxPanel* CreateFinishPage(wxWindow* parent);

    void RefreshProtocolDetail();
    void RefreshModePage();
    void RefreshRolePage();
    void RefreshParamsPage();

    wxWindow* CreateControl(wxWindow* parent, const FieldDescriptor& fd);
    void SaveCurrentPageData();
    void UpdateButtonsState();
    void OnNext(wxCommandEvent& evt);
    void OnBack(wxCommandEvent& evt);

    void OnProtocolSelected(wxCommandEvent& evt);
    void OnModeRadioChanged(wxCommandEvent& evt);
    void OnRoleRadioChanged(wxCommandEvent& evt);

    void OnPaint(wxPaintEvent& evt);
    void OnMouseDown(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnMouseUp(wxMouseEvent& evt);

    wxPoint m_dragStart;
    wxPoint m_winStart;

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
