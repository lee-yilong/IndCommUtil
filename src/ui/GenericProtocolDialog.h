#pragma once

#include <wx/radiobox.h>
#include <wx/wx.h>

#include <map>
#include <vector>

#include "core/IProtocol.h"

namespace icu {

// 数据驱动对话框：根据 IProtocol 的字段描述自动生成控件
// 新增协议无需写新的对话框
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

    std::vector<wxPanel*> m_modePanels;
    std::vector<std::map<std::string, wxWindow*>> m_controlsPerMode;

    void BuildUi();
    wxPanel* BuildFieldsPanel(wxWindow* parent, const std::vector<FieldDescriptor>& fields,
                              std::map<std::string, wxWindow*>& outControls);
    wxWindow* CreateControl(wxWindow* parent, const FieldDescriptor& fd);

    void ApplyInitialValues(int modeIndex);
    void CollectValues(int modeIndex, std::map<std::string, FieldValue>& out) const;

    void OnModeChanged(wxCommandEvent& evt);

    wxDECLARE_EVENT_TABLE();
};

}  // namespace icu
