#include "stdafx.h"
#include "DlgDebug.h"

#define COL_LEN_LEVEL 50
#define COL_LEN_CMB 80
#define COL_LEN_IGNORE 65

#define COL_FN_NAME "Part of function name"
#define COL_FN_INCL "Filter"
#define COL_FN_LEVEL "Level"
#define COL_FN_IGNORE "Ignore"

#define COL_MODULE_NAME "Module name"
#define COL_MODULE_IGNORE "Ignore"

#define COL_SRC_PATH "Part of source path"
#define COL_SRC_INCL "Filter"
#define COL_SRC_LEVEL "Level"
#define COL_SRC_IGNORE "Ignore"
//#define COL_SRC_INCL "Excl src"

LRESULT DlgDebug::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());
	m_btnApplyFuncFIlters.Attach(GetDlgItem(IDC_CHK_APPLY_FUNC_FILTERS));
	m_btnApplyPathFIlters.Attach(GetDlgItem(IDC_CHK_APPLY_PATH_FILTERS));
	m_staticFilterList.Attach(GetDlgItem(IDC_STATIC_FITER_LST));
	m_staticModulList.Attach(GetDlgItem(IDC_STATIC_MODUL_LST));
	m_staticSrcList.Attach(GetDlgItem(IDC_STATIC_SRC_LST));
	m_lblFilterName.Attach(GetDlgItem(IDC_STATIC_FITER_FILE));

	m_btnApplyFuncFIlters.SetCheck(gSettings.m_DbgSettings.m_applyFuncFilters ? BST_CHECKED : BST_UNCHECKED);
	m_btnApplyPathFIlters.SetCheck(gSettings.m_DbgSettings.m_applyPathFilters ? BST_CHECKED : BST_UNCHECKED);
	m_lblFilterName.SetWindowText(gSettings.DbgSettingsPath());

	CRect rc;
	m_staticFilterList.GetClientRect(&rc);
	m_FilterGrid.Create(m_staticFilterList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_FilterGrid.SetListener(this);
	m_FilterGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_FilterGrid.AddColumn(COL_FN_NAME, m_FilterGrid.GetGridClientWidth() - COL_LEN_CMB - COL_LEN_LEVEL - COL_LEN_IGNORE, CGridCtrl::EDIT_TEXT);
	m_FilterGrid.AddColumn(COL_FN_INCL, COL_LEN_CMB, CGridCtrl::EDIT_DROPDOWNLIST, CGridCtrl::LEFT, VT_I4);
    m_FilterGrid.AddColumnLookup(COL_FN_INCL, 0, "Skip");
	m_FilterGrid.AddColumnLookup(COL_FN_INCL, 1, "Show");
	m_FilterGrid.AddColumnLookup(COL_FN_INCL, 2, "Hide");
	m_FilterGrid.AddColumnLookup(COL_FN_INCL, 3, "Expand");
	m_FilterGrid.AddColumn(COL_FN_LEVEL, COL_LEN_LEVEL, CGridCtrl::EDIT_DROPDOWNLIST, CGridCtrl::LEFT, VT_I4);
	for (char i = 0; i < 10; i++) {
		char ch[2] = { '0' + i, 0 };
		m_FilterGrid.AddColumnLookup(COL_FN_LEVEL, i, ch);
	}
	m_FilterGrid.AddColumn(COL_FN_IGNORE, COL_LEN_IGNORE, CGridCtrl::EDIT_CHECK);

	m_staticModulList.GetClientRect(&rc);
	m_ModulGrid.Create(m_staticModulList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_ModulGrid.SetListener(this);
	m_ModulGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_ModulGrid.GetClientRect(&rc);
	m_ModulGrid.AddColumn(COL_MODULE_NAME, m_ModulGrid.GetGridClientWidth() - COL_LEN_IGNORE, CGridCtrl::EDIT_TEXT);
	m_ModulGrid.AddColumn(COL_MODULE_IGNORE, COL_LEN_IGNORE, CGridCtrl::EDIT_CHECK);

	m_staticSrcList.GetClientRect(&rc);
	m_SrcGrid.Create(m_staticSrcList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_SrcGrid.SetListener(this);
	m_SrcGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_SrcGrid.AddColumn(COL_SRC_PATH, m_SrcGrid.GetGridClientWidth() - COL_LEN_CMB - COL_LEN_LEVEL - COL_LEN_IGNORE, CGridCtrl::EDIT_TEXT);
	m_SrcGrid.AddColumn(COL_SRC_INCL, COL_LEN_CMB, CGridCtrl::EDIT_DROPDOWNLIST, CGridCtrl::LEFT, VT_I4);
	m_SrcGrid.AddColumnLookup(COL_SRC_INCL, 0, "Skip");
	m_SrcGrid.AddColumnLookup(COL_SRC_INCL, 1, "Show");
	m_SrcGrid.AddColumnLookup(COL_SRC_INCL, 2, "Hide");
	m_SrcGrid.AddColumn(COL_SRC_LEVEL, COL_LEN_LEVEL, CGridCtrl::EDIT_DROPDOWNLIST, CGridCtrl::LEFT, VT_I4);
	for (char i = 0; i < 10; i++) {
		char ch[2] = { '0' + i, 0}; 
		m_SrcGrid.AddColumnLookup(COL_SRC_LEVEL, i, ch);
	}
	m_SrcGrid.AddColumn(COL_SRC_IGNORE, COL_LEN_IGNORE, CGridCtrl::EDIT_CHECK);

	FillGrids(gSettings.m_DbgSettings);

	return TRUE;
}

void DlgDebug::FillGrids(DbgSettings &dbgSettings)
{
	std::vector<DbgFilter>& arDbgFilter = dbgSettings.m_arDbgFilter;
	m_FilterGrid.SetRedraw(FALSE);
	m_FilterGrid.DeleteAllItems();
	for (size_t i = 0; i < arDbgFilter.size(); i++)
	{
		long nItem = m_FilterGrid.AddRow();
		m_FilterGrid.SetItem(nItem, COL_FN_NAME, arDbgFilter[i].m_szFunc);
        int showFunc = arDbgFilter[i].m_showFunc;
        if (showFunc < 0) showFunc = 0;
        if (showFunc > 3) showFunc = 3;
        m_FilterGrid.SetItem(nItem, COL_FN_INCL, showFunc);
		m_FilterGrid.SetItem(nItem, COL_FN_LEVEL, arDbgFilter[i].m_Priority);
		m_FilterGrid.SetItem(nItem, COL_FN_IGNORE, arDbgFilter[i].m_skipFilter);
	}
	m_FilterGrid.SetRedraw(TRUE);

	std::vector<DbgSource>& arDbgSourcer = dbgSettings.m_arDbgSources;
	m_SrcGrid.SetRedraw(FALSE);
	m_SrcGrid.DeleteAllItems();
	for (size_t i = 0; i < arDbgSourcer.size(); i++)
	{
		long nItem = m_SrcGrid.AddRow();
		m_SrcGrid.SetItem(nItem, COL_SRC_PATH, arDbgSourcer[i].m_szSrc);

		int showSrc = arDbgSourcer[i].m_showSrc;
		if (showSrc < 0) showSrc = 0;
		if (showSrc > 2) showSrc = 2;
		m_SrcGrid.SetItem(nItem, COL_SRC_INCL, showSrc);
		m_SrcGrid.SetItem(nItem, COL_SRC_LEVEL, arDbgSourcer[i].m_Priority);
		m_SrcGrid.SetItem(nItem, COL_SRC_IGNORE, arDbgSourcer[i].m_skipSrc);
	}
	m_SrcGrid.SetRedraw(TRUE);

	std::vector<DbgModule>& arDbgModule = dbgSettings.m_arDbgModules;
	m_ModulGrid.SetRedraw(FALSE);
	m_ModulGrid.DeleteAllItems();
	for (size_t i = 0; i < arDbgModule.size(); i++)
	{
		long nItem = m_ModulGrid.AddRow();
		m_ModulGrid.SetItem(nItem, COL_MODULE_NAME, arDbgModule[i].m_szModul);
		m_ModulGrid.SetItem(nItem, COL_MODULE_IGNORE, arDbgModule[i].m_skipModule);
	}
	m_ModulGrid.SetRedraw(TRUE);
}

bool DlgDebug::WriteDbgSettings(char* szFilterPath, DbgSettings &dbgSettings)
{
	dbgSettings.m_applyFuncFilters = m_btnApplyFuncFIlters.GetCheck();
	dbgSettings.m_applyPathFilters = m_btnApplyPathFIlters.GetCheck();

	int count = m_FilterGrid.GetRowCount();
	for (int i = 0; i < count; i++)
	{
		_variant_t vt = m_FilterGrid.GetItem(i, COL_FN_NAME);
		if (vt.vt == VT_BSTR && vt.bstrVal)
		{
			char* szFnName = _com_util::ConvertBSTRToString(vt.bstrVal);
			DbgFilter dbgFilter;
			strncpy_s(dbgFilter.m_szFunc, szFnName, _countof(dbgFilter.m_szFunc) - 1);
			dbgFilter.m_szFunc[_countof(dbgFilter.m_szFunc) - 1] = 0;

			vt = m_FilterGrid.GetItem(i, COL_FN_INCL);
			dbgFilter.m_showFunc = (long)vt;

			vt = m_FilterGrid.GetItem(i, COL_FN_LEVEL);
			dbgFilter.m_Priority = (long)vt;

			vt = m_FilterGrid.GetItem(i, COL_FN_IGNORE);
			dbgFilter.m_skipFilter = vt.pcVal[0] != '0';

			dbgSettings.m_arDbgFilter.push_back(dbgFilter);
			delete szFnName;
		}
	}
	count = m_SrcGrid.GetRowCount();
	for (int i = 0; i < count; i++)
	{
		_variant_t vt = m_SrcGrid.GetItem(i, COL_SRC_PATH);
		if (vt.vt == VT_BSTR && vt.bstrVal)
		{
			char* szSrc = _com_util::ConvertBSTRToString(vt.bstrVal);
			DbgSource dbgSource;
			strncpy_s(dbgSource.m_szSrc, szSrc, _countof(dbgSource.m_szSrc) - 1);
			dbgSource.m_szSrc[_countof(dbgSource.m_szSrc) - 1] = 0;

			vt = m_SrcGrid.GetItem(i, COL_SRC_INCL);
			dbgSource.m_showSrc = (long)vt;

			vt = m_SrcGrid.GetItem(i, COL_SRC_LEVEL);
			dbgSource.m_Priority = (long)vt;

			vt = m_SrcGrid.GetItem(i, COL_SRC_IGNORE);
			dbgSource.m_skipSrc = vt.pcVal[0] != '0';
			

			dbgSettings.m_arDbgSources.push_back(dbgSource);
			delete szSrc;
		}
	} 

	count = m_ModulGrid.GetRowCount();
	for (int i = 0; i < count; i++)
	{
		_variant_t vt = m_ModulGrid.GetItem(i, COL_MODULE_NAME);
		if (vt.vt == VT_BSTR && vt.bstrVal)
		{
			DbgModule dbgModule;
			char* szModul = _com_util::ConvertBSTRToString(vt.bstrVal);
			strncpy_s(dbgModule.m_szModul, szModul, _countof(dbgModule.m_szModul) - 1);
			dbgModule.m_szModul[_countof(dbgModule.m_szModul) - 1] = 0;

			vt = m_ModulGrid.GetItem(i, COL_MODULE_IGNORE);
			dbgModule.m_skipModule = vt.pcVal[0] != '0';
			
			dbgSettings.m_arDbgModules.push_back(dbgModule);
			delete szModul;
        }
	}
	return gSettings.WriteDbgSettings(szFilterPath, dbgSettings);
}

LRESULT DlgDebug::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndEditing(wID != IDOK);

	if (wID == IDOK)
	{
		//SAVE
		CHAR  szFilterPath[MAX_PATH + 1];
		m_lblFilterName.GetWindowText(szFilterPath, MAX_PATH);

		CFileDialog dlg(FALSE);
		if (szFilterPath[0] == 0)
		{
			if (!dlg.DoModal())
				return 0;
			strcpy_s(szFilterPath, dlg.m_ofn.lpstrFile);
		}

		DbgSettings dbgSettings;
		if (!WriteDbgSettings(szFilterPath, dbgSettings))
			return 0;

		gSettings.SetDbgSettings(szFilterPath, dbgSettings);
	}
	EndDialog(wID);
	return 0;
}

LRESULT DlgDebug::OnBnClickedButtonOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog dlg(TRUE);
	if (!dlg.DoModal() || !dlg.m_ofn.lpstrFile[0])
		return 0;

	DbgSettings dbgSettings;
	if (!gSettings.ReadDbgSettings(dlg.m_ofn.lpstrFile, dbgSettings))
	{
		MessageBox(TEXT("Could not load debugger settings"), TEXT("Flow Trace Error"), MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}
	m_lblFilterName.SetWindowText(dlg.m_ofn.lpstrFile);

	FillGrids(dbgSettings);
	m_btnApplyFuncFIlters.SetCheck(dbgSettings.m_applyFuncFilters ? BST_CHECKED : BST_UNCHECKED);
	m_btnApplyPathFIlters.SetCheck(dbgSettings.m_applyPathFilters ? BST_CHECKED : BST_UNCHECKED);
	return 0;
}


LRESULT DlgDebug::OnBnClickedButtonsaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CFileDialog dlg(FALSE);
	if (!dlg.DoModal())
		return 0;

	EndEditing(false);

	DbgSettings dbgSettings;
	if (!WriteDbgSettings(dlg.m_ofn.lpstrFile, dbgSettings))
		return 0;

	m_lblFilterName.SetWindowText(dlg.m_ofn.lpstrFile);

	return 0;
}

void DlgDebug::EndEditing(bool abort)
{
	m_FilterGrid.EndEdit(abort);
	m_ModulGrid.EndEdit(abort);
	m_SrcGrid.EndEdit(abort);
}