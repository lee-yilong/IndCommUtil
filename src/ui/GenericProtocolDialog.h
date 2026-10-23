#pragma once

#include <wx/radiobox.h>
#include <wx/wx.h>

#include <map>
#include <vector>

#include "core/IProtocol.h"

namespace icu {

// 数据驱动对话框：根据 IProtocol 的字段描述自动生成控件
class GenericProtocolDialog : public wxDialog {
public:
    GenericProtocolDialog(wxWindow* parent, IProtocol* protocol, int initialModeIndex,
                          const std::map<std::string, FieldValue>& initialValues);

    int GetModeIndex() const;
    std::map<std::string, FieldValue> GetValues() const;

private:
    IProtocol* m_protocol = nullptr;
    int m_initialMode = 0;
    std::map<std::string, FieldValue> m_initialValues;

    wxRadioBox* m_modeRadio = nullptr;
    wxBoxSizer* m_mainSizer = nullptr;
    wxSizer* m_dynamicSizer = nullptr;

    int m_activeModeIndex = -1;
    wxPanel* m_currentPanel = nullptr;

    // 缓存已生成的控件，避免重复销毁重建
    std::vector<std::map<std::string, wxWindow*>> m_controlsPerMode;

    void BuildUi();
    void RebuildFieldsPanel(int modeIndex);
    wxPanel* CreateFieldsPanel(wxWindow* parent, const std::vector<FieldDescriptor>& fields,
                               std::map<std::string, wxWindow*>& outControls);
    wxWindow* CreateControl(wxWindow* parent, const FieldDescriptor& fd);

    void ApplyInitialValues(int modeIndex, std::map<std::string, wxWindow*>& controls);
    void CollectValues(int modeIndex, const std::map<std::string, wxWindow*>& controls,
                       std::map<std::string, FieldValue>& out) const;

    void OnModeChanged(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
