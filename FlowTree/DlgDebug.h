#pragma once
#include "Settings.h"
#include "resource.h"

class DlgDebug : public CDialogImpl<DlgDebug>, public CGridCtrl::CListener
{
public:
	enum { IDD = IDD_DEBUG };

	CGridCtrl m_FilterGrid;
	CGridCtrl m_ModulGrid;
	CGridCtrl m_SrcGrid;
	CButton m_btnApplyFuncFIlters;
	CButton m_btnApplyPathFIlters;
	CStatic m_staticFilterList;//	IDC_STATIC_FITER_LST
	CStatic m_staticModulList;//	IDC_STATIC_MODUL_LST
	CStatic m_staticSrcList;//	IDC_STATIC_SRC_LST
	CStatic m_lblFilterName;//	IDC_STATIC_FITER_FILE

	BEGIN_MSG_MAP(DlgFilter)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)

		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_HANDLER(IDC_BUTTON_OPEN, BN_CLICKED, OnBnClickedButtonOpen)
		COMMAND_HANDLER(IDC_BUTTONSAVE_AS, BN_CLICKED, OnBnClickedButtonsaveAs)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedButtonOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBnClickedButtonsaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void FillGrids(DbgSettings &dbgSettings);
	bool WriteDbgSettings(char* szFilterPath, DbgSettings &dbgSettings);
	void EndEditing(bool abort);
};
