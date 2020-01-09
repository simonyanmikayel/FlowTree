#include "stdafx.h"
#include "DlgDebug.h"

#define COL_LEN_CHK 70
#define COL_LEN_CMB 100
#define COL_FN_NAME "Part of function name"
#define COL_INCL_FN_CALL "Preserv fn"
#define COL_APPLY_FILTER "Skip filter"
#define COL_MODULE_NAME "Module name"
#define COL_SRC_PATH "Part of source path"
#define COL_INCL_SRC "Excl src"

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
	m_lblFilterName.SetWindowText(gSettings.GetDbgSettingsPath());

	CRect rc;
	m_staticFilterList.GetClientRect(&rc);
	m_FilterGrid.Create(m_staticFilterList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_FilterGrid.SetListener(this);
	m_FilterGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_FilterGrid.AddColumn(COL_FN_NAME, m_FilterGrid.GetGridClientWidth() - (COL_LEN_CMB + COL_LEN_CHK), CGridCtrl::EDIT_TEXT);
	m_FilterGrid.AddColumn(COL_INCL_FN_CALL, COL_LEN_CMB, CGridCtrl::EDIT_DROPDOWNLIST, CGridCtrl::LEFT, VT_I4);
    m_FilterGrid.AddColumnLookup(COL_INCL_FN_CALL, 0, "Skip func");
    m_FilterGrid.AddColumnLookup(COL_INCL_FN_CALL, 1, "Skip Inners");
    m_FilterGrid.AddColumnLookup(COL_INCL_FN_CALL, 2, "Skip All");
	m_FilterGrid.AddColumn(COL_APPLY_FILTER, COL_LEN_CHK, CGridCtrl::EDIT_CHECK);

	m_staticModulList.GetClientRect(&rc);
	m_ModulGrid.Create(m_staticModulList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_ModulGrid.SetListener(this);
	m_ModulGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_ModulGrid.GetClientRect(&rc);
	m_ModulGrid.AddColumn(COL_MODULE_NAME, m_ModulGrid.GetGridClientWidth(), CGridCtrl::EDIT_TEXT);

	m_staticSrcList.GetClientRect(&rc);
	m_SrcGrid.Create(m_staticSrcList.m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE, IDC_FITER_GRID);
	m_SrcGrid.SetListener(this);
	m_SrcGrid.SetExtendedGridStyle(GS_EX_CONTEXTMENU);
	m_SrcGrid.AddColumn(COL_SRC_PATH, m_SrcGrid.GetGridClientWidth() - (1 * COL_LEN_CHK), CGridCtrl::EDIT_TEXT);
	m_SrcGrid.AddColumn(COL_INCL_SRC, COL_LEN_CHK, CGridCtrl::EDIT_CHECK);
	
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
        if (showFunc > 2) showFunc = 2;
        m_FilterGrid.SetItem(nItem, COL_INCL_FN_CALL, showFunc);
		m_FilterGrid.SetItem(nItem, COL_APPLY_FILTER, arDbgFilter[i].m_skipFilter);
	}
	m_FilterGrid.SetRedraw(TRUE);

	std::vector<DbgSource>& arDbgSourcer = dbgSettings.m_arDbgSources;
	m_SrcGrid.SetRedraw(FALSE);
	m_SrcGrid.DeleteAllItems();
	for (size_t i = 0; i < arDbgSourcer.size(); i++)
	{
		long nItem = m_SrcGrid.AddRow();
		m_SrcGrid.SetItem(nItem, COL_SRC_PATH, arDbgSourcer[i].m_szSrc);
		m_SrcGrid.SetItem(nItem, COL_INCL_SRC, arDbgSourcer[i].m_exclSrc);
	} 
	m_SrcGrid.SetRedraw(TRUE);

	std::vector<DbgModule>& arDbgModuler = dbgSettings.m_arDbgModules;
	m_ModulGrid.SetRedraw(FALSE);
	m_ModulGrid.DeleteAllItems();
	for (size_t i = 0; i < arDbgModuler.size(); i++)
	{
		long nItem = m_ModulGrid.AddRow();
		m_ModulGrid.SetItem(nItem, COL_MODULE_NAME, arDbgModuler[i].m_szModul);
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
		_variant_t vtFnName = m_FilterGrid.GetItem(i, COL_FN_NAME);
		if (vtFnName.vt == VT_BSTR && vtFnName.bstrVal)
		{
			char* szFnName = _com_util::ConvertBSTRToString(vtFnName.bstrVal);
			DbgFilter dbgFilter;
			strncpy_s(dbgFilter.m_szFunc, szFnName, _countof(dbgFilter.m_szFunc) - 1);
			dbgFilter.m_szFunc[_countof(dbgFilter.m_szFunc) - 1] = 0;
			_variant_t vtInclFn = m_FilterGrid.GetItem(i, COL_INCL_FN_CALL);
			dbgFilter.m_showFunc = (long)vtInclFn;
			_variant_t vtApply = m_FilterGrid.GetItem(i, COL_APPLY_FILTER);
			dbgFilter.m_skipFilter = vtApply.pcVal[0] != '0';
			dbgSettings.m_arDbgFilter.push_back(dbgFilter);
			delete szFnName;
		}
	}
	count = m_SrcGrid.GetRowCount();
	for (int i = 0; i < count; i++)
	{
		_variant_t vtSrc = m_SrcGrid.GetItem(i, COL_SRC_PATH);
		if (vtSrc.vt == VT_BSTR && vtSrc.bstrVal)
		{
			char* szSrc = _com_util::ConvertBSTRToString(vtSrc.bstrVal);
			DbgSource dbgSource;
			strncpy_s(dbgSource.m_szSrc, szSrc, _countof(dbgSource.m_szSrc) - 1);
			dbgSource.m_szSrc[_countof(dbgSource.m_szSrc) - 1] = 0;
			_variant_t vtInclSrc = m_SrcGrid.GetItem(i, COL_INCL_SRC);
			dbgSource.m_exclSrc = vtInclSrc.pcVal[0] != '0';
			dbgSettings.m_arDbgSources.push_back(dbgSource);
			delete szSrc;
		}
	} 
	count = m_ModulGrid.GetRowCount();
	for (int i = 0; i < count; i++)
	{
		_variant_t vtModuleName = m_ModulGrid.GetItem(i, COL_MODULE_NAME);
		if (vtModuleName.vt == VT_BSTR && vtModuleName.bstrVal)
		{
			DbgModule dbgModule;
			char* szModul = _com_util::ConvertBSTRToString(vtModuleName.bstrVal);
			strncpy_s(dbgModule.m_szModul, szModul, _countof(dbgModule.m_szModul) - 1);
			dbgModule.m_szModul[_countof(dbgModule.m_szModul) - 1] = 0;
			dbgSettings.m_arDbgModules.push_back(dbgModule);
            delete szModul;
        }
	}
	return gSettings.WriteDbgSettings(szFilterPath, dbgSettings);
}

LRESULT DlgDebug::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_FilterGrid.IsEditing() || m_ModulGrid.IsEditing() || m_SrcGrid.IsEditing())
	{
		if (m_FilterGrid.IsEditing())
			m_FilterGrid.EndEdit(wID != IDOK);
		else if (m_ModulGrid.IsEditing())
			m_ModulGrid.EndEdit(wID != IDOK);
		else
			m_SrcGrid.EndEdit(wID != IDOK);
		return 0;
	}

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
	if (!dlg.DoModal())
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

	DbgSettings dbgSettings;
	if (!WriteDbgSettings(dlg.m_ofn.lpstrFile, dbgSettings))
		return 0;

	m_lblFilterName.SetWindowText(dlg.m_ofn.lpstrFile);

	return 0;
}