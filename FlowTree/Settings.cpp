#include "StdAfx.h"
#include "Settings.h"
#include "resource.h"
#include "helpers.h"
#include <SimpleIni.h>

CSettings gSettings;

// registry keys and values
LPCTSTR STR_APP_REG_KEY = _T("Software\\TermianlTools\\FlowTrace");
LPCTSTR STR_APP_REG_VAL_WINDOWPOS = _T("WindowPos");

LPCTSTR STR_APP_REG_VAL_FONTNAME = _T("FontName_1");
LPCTSTR STR_APP_REG_VAL_FONTWIDTH = _T("FontWeight");
LPCTSTR STR_APP_REG_VAL_FONTSIZE = _T("FontSize");
LPCTSTR STR_APP_REG_VAL_SEARCH_LIST = _T("SearchList");

LPCTSTR STR_APP_REG_VAL_DbgSettingsPath = _T("DbgSettingsPath");
LPCTSTR STR_APP_REG_VAL_QtCreatorPath = _T("QtCreatorPath");
LPCTSTR STR_APP_REG_VAL_CLionPath = _T("CLionPath");

#define DefFontSize 11

static CHAR* DEF_FONT_NAME = _T("Consolas"); //Courier New //Consolas //Inconsolata

CSettings::CSettings() :
	CRegKeyExt(STR_APP_REG_KEY)
	, m_VertSplitterPos(this, _T("VertSplitterPos"), 50)
	, m_HorzSplitterPos(this, _T("HorzSplitterPos"), 50)
	, m_StackSplitterPos(this, _T("StackSplitterPos"), 50)
	, m_FlowTracesHiden(this, _T("FlowTracesHiden"), TRUE)
	, m_TreeViewHiden(this, _T("TreeViewHiden"), TRUE)
	, m_InfoHiden(this, _T("InfoHiden"), TRUE)
	, m_ShowElapsedTime(this, _T("ShowElapsedTime"), FALSE)
	, m_FullSrcPath(this, _T("FullSrcPath"), FALSE)
	, m_FullFnName(this, _T("FullFnName"), FALSE)
	, m_LogOnlyTraces(this, _T("LogOnlyTraces"), FALSE)
	, m_DissableBufferization(this, _T("DissableBufferization"), FALSE)
	, m_UseTcpForLog(this, _T("UseTcpForLog"), FALSE)
	, m_IdeType(this, _T("IdeType"), 0)
	, m_ColNN(this, _T("ColNN"), 0)
	, m_ColApp(this, _T("ColApp"), 0)
	, m_ColPID(this, _T("ColPID"), 0)
	, m_ColThreadNN(this, _T("ColThreadNN"), 1)
	, m_ColFunc(this, _T("ColFunc"), 1)
	, m_ColLine(this, _T("ColLine"), 0)
	, m_ColTime(this, _T("ColTime"), 1)
	, m_ColCallAddr(this, _T("ColCallAddr"), 0)
	, m_FnCallLine(this, _T("FnCallLine"), 0)
{
	


	InitFont();

	Read(STR_APP_REG_VAL_DbgSettingsPath, m_DbgSettingsPath, MAX_PATH, "");
	ReadDbgSettings(m_DbgSettingsPath, m_DbgSettings);
	
	m_QtCreatorPath[0] = 0;
	Read(STR_APP_REG_VAL_QtCreatorPath, m_QtCreatorPath, MAX_PATH, "");
	if (!m_QtCreatorPath[0])
		SetQtCreatorPath("C:\\Qt\\Tools\\QtCreator\\bin\\qtcreator.exe");
	Read(STR_APP_REG_VAL_CLionPath, m_CLionPath, MAX_PATH, "");
}

bool CSettings::CheckUIFont(HDC hdc)
{
	bool ok = true;
	return ok;
}

CSettings::~CSettings()
{
	DeleteFont();
	RemoveFontMemResourceEx(m_resourceFonthandle);
}

void CSettings::AddDefaultFont()
{
	HINSTANCE hResInstance = _Module.m_hInst;

	HRSRC res = FindResource(hResInstance, MAKEINTRESOURCE(IDR_FONT1), _T("FONTS"));
	if (res)
	{
		HGLOBAL mem = LoadResource(hResInstance, res);
		void *data = LockResource(mem);
		DWORD len = SizeofResource(hResInstance, res);

		DWORD nFonts;
		m_resourceFonthandle = AddFontMemResourceEx(
			data,       // font resource
			len,       // number of bytes in font resource 
			NULL,          // Reserved. Must be 0.
			&nFonts      // number of fonts installed
		);
	}
}

void CSettings::SetDbgSettings(char* filePath, DbgSettings& dbgSettings)
{
	m_DbgSettings = dbgSettings;
	strcpy_s(m_DbgSettingsPath, filePath);
	Write(STR_APP_REG_VAL_DbgSettingsPath, filePath);
}

void CSettings::SetQtCreatorPath(const char* filePath)
{
	strcpy_s(m_QtCreatorPath, filePath);
	Write(STR_APP_REG_VAL_QtCreatorPath, filePath);
}

void CSettings::SetCLionPath(const char* filePath)
{
	strcpy_s(m_CLionPath, filePath);
	Write(STR_APP_REG_VAL_CLionPath, filePath);
}

bool CSettings::WriteDbgSettings(char* FilterPath, DbgSettings& dbgSettings)
{
	CSimpleIniA ini(false, true, false);
	ini.SetBoolValue("Common", "ApplyFuncFilters", dbgSettings.m_applyFuncFilters ? true : false);
	ini.SetBoolValue("Common", "ApplyPathFilters", dbgSettings.m_applyPathFilters ? true : false);

	for (size_t i = 0; i < dbgSettings.m_arDbgFilter.size(); i++)
	{
		char szFilterValue[MAX_FILTER_ITEM_BUF];
		dbgSettings.m_arDbgFilter[i].EncodeText(szFilterValue);
		ini.SetValue("Filters", "f", szFilterValue);
	}
	for (size_t i = 0; i < dbgSettings.m_arDbgSources.size(); i++)
	{
		char szSrcValue[MAX_FILTER_ITEM_BUF];
		dbgSettings.m_arDbgSources[i].EncodeText(szSrcValue);
		ini.SetValue("Sources", "s", szSrcValue);
	}
	for (size_t i = 0; i < dbgSettings.m_arDbgModules.size(); i++)
	{
		char szModuleValue[MAX_MODUL_ITEM_BUF];
		dbgSettings.m_arDbgModules[i].EncodeText(szModuleValue);
		ini.SetValue("Modules", "m", szModuleValue);
	}

	return SI_OK == ini.SaveFile(FilterPath);
}

bool CSettings::ReLoadDbgSettings()
{
	if (!m_DbgSettingsPath[0])
		return false;
	if (!ReadDbgSettings(m_DbgSettingsPath, m_DbgSettings))
	{
		::MessageBox(hwndMain, TEXT("Could not reload debugger settings"), TEXT("Flow Trace Error"), MB_OK | MB_ICONEXCLAMATION);
		return false;
	}
	return true;
}

bool CSettings::ReadDbgSettings(char* FilterPath, DbgSettings& dbgSettings)
{
	dbgSettings.m_arDbgFilter.clear();
	dbgSettings.m_arDbgModules.clear();
	dbgSettings.m_arDbgSources.clear();
	dbgSettings.m_applyFuncFilters = 1;
	dbgSettings.m_applyPathFilters = 1;

	CSimpleIniA ini(false, true, false);
	if (SI_OK != ini.LoadFile(FilterPath))
		return false;

	dbgSettings.m_applyFuncFilters = ini.GetBoolValue("Common", "ApplyFuncFilters");
	dbgSettings.m_applyPathFilters = ini.GetBoolValue("Common", "ApplyPathFilters");

	CSimpleIniA::TNamesDepend filterValues;
	ini.GetAllValues("Filters", "f", filterValues);
	CSimpleIniA::TNamesDepend::const_iterator itFilter;
	for (itFilter = filterValues.begin(); itFilter != filterValues.end(); ++itFilter) {
		DbgFilter  dbgFilter;
		dbgFilter.DecodeText(itFilter->pItem);
		dbgSettings.m_arDbgFilter.push_back(dbgFilter);
	}

	CSimpleIniA::TNamesDepend srcValues;
	ini.GetAllValues("Sources", "s", srcValues);
	CSimpleIniA::TNamesDepend::const_iterator itSrc;
	for (itSrc = srcValues.begin(); itSrc != srcValues.end(); ++itSrc) {
		DbgSource  dbgSource;
		dbgSource.DecodeText(itSrc->pItem);
		dbgSettings.m_arDbgSources.push_back(dbgSource);
	}

	CSimpleIniA::TNamesDepend moduleValues;
	ini.GetAllValues("Modules", "m", moduleValues);
	CSimpleIniA::TNamesDepend::const_iterator itModule;
	for (itModule = moduleValues.begin(); itModule != moduleValues.end(); ++itModule) {
		DbgModule  dbgModule;
		dbgModule.DecodeText(itModule->pItem);
		dbgSettings.m_arDbgModules.push_back(dbgModule);
	}
	return true;
}

void CSettings::InitFont()
{
	DeleteFont();

	ZeroMemory(&m_logFont, sizeof(LOGFONT));

	Read(STR_APP_REG_VAL_FONTNAME, m_logFont.lfFaceName, LF_FACESIZE, DEF_FONT_NAME);
	Read(STR_APP_REG_VAL_FONTWIDTH, m_logFont.lfWeight, FW_NORMAL);
	Read(STR_APP_REG_VAL_FONTSIZE, m_FontSize.val, DefFontSize);

	m_FontName = m_logFont.lfFaceName;
	m_FontWeight = m_logFont.lfWeight;

	if (m_FontSize <= 4) m_FontSize = DefFontSize;

	m_logFont.lfWeight = (m_logFont.lfWeight <= FW_MEDIUM) ? FW_NORMAL : FW_BOLD;

	HDC hdc = CreateIC(TEXT("DISPLAY"), NULL, NULL, NULL);
	m_logFont.lfHeight = -MulDiv(m_FontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	m_logFont.lfQuality = CLEARTYPE_NATURAL_QUALITY; //ANTIALIASED_QUALITY

	m_Font = CreateFontIndirect(&m_logFont);

	TEXTMETRIC  tm;
	SelectObject(hdc, m_Font);
	GetTextMetrics(hdc, &tm);
	m_FontHeight = tm.tmHeight + tm.tmExternalLeading;
	m_FontWidth = tm.tmAveCharWidth;

	DeleteDC(hdc);
}

void CSettings::SetUIFont(CHAR* lfFaceName, LONG lfWeight, LONG fontSize)
{
	Write(STR_APP_REG_VAL_FONTNAME, lfFaceName);
	Write(STR_APP_REG_VAL_FONTWIDTH, lfWeight);
	Write(STR_APP_REG_VAL_FONTSIZE, fontSize);
	InitFont();
}

void CSettings::DeleteFont()
{
	if (m_Font) {
		DeleteObject(m_Font);
		m_Font = NULL;
	}
}

void CSettings::RestoreWindPos(HWND hWnd)
{
	WINDOWPLACEMENT wpl;
	if (Read(STR_APP_REG_VAL_WINDOWPOS, &wpl, sizeof(wpl))) {
		RECT rcWnd = wpl.rcNormalPosition;
		int cx, cy, x, y;
		cx = rcWnd.right - rcWnd.left;
		cy = rcWnd.bottom - rcWnd.top;
		x = rcWnd.left;
		y = rcWnd.top;

		// Get the monitor info
		MONITORINFO monInfo;
		HMONITOR hMonitor = ::MonitorFromPoint(CPoint(x, y), MONITOR_DEFAULTTONEAREST);
		monInfo.cbSize = sizeof(MONITORINFO);
		if (::GetMonitorInfo(hMonitor, &monInfo))
		{
			// Adjust for work area
			x += monInfo.rcWork.left - monInfo.rcMonitor.left;
			y += monInfo.rcWork.top - monInfo.rcMonitor.top;
			// Ensure top left point is on screen
			if (CRect(monInfo.rcWork).PtInRect(CPoint(x, y)) == FALSE)
			{
				x = monInfo.rcWork.left;
				y = monInfo.rcWork.top;
			}
		}
		else
		{
			RECT rcScreen;
			SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0);

			cx = min(rcScreen.right, cx);
			cy = min(rcScreen.bottom, cy);
			x = max(0, min(x, rcScreen.right - cx));
			y = max(0, min(y, rcScreen.bottom - cy));
		}

		::SetWindowPos(hWnd, 0, x, y, cx, cy, SWP_NOZORDER);

		if (wpl.flags & WPF_RESTORETOMAXIMIZED)
		{
			//::ShowWindow(hWnd, SW_SHOWMAXIMIZED);
			::PostMessage(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
		}
	}
}

void CSettings::SaveWindPos(HWND hWnd)
{
	WINDOWPLACEMENT wpl = { sizeof(WINDOWPLACEMENT) };
	if (::GetWindowPlacement(hWnd, &wpl))
		Write(STR_APP_REG_VAL_WINDOWPOS, &wpl, sizeof(wpl));
}

void CSettings::SetConsoleColor(int MsgType, DWORD& textColor, DWORD& bkColor)
{
	enum QtMsgType { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg };
	if (MsgType == QtInfoMsg) {
		textColor = RGB(0, 255, 0);//32	Green
		//bkColor = RGB(0, 0, 0);//40	Black
	}
	else if (MsgType == QtFatalMsg || MsgType == QtCriticalMsg) {
		textColor = RGB(255, 0, 0);//31	Red
		//bkColor = RGB(0, 0, 0);//40	Black
	}
	else if (MsgType == QtWarningMsg) {
		textColor = RGB(255, 255, 0);//33	Yellow
		//bkColor = RGB(0, 0, 0);//40	Black
	}
	else { //MsgType == QtDebugMsg
		// do nothing
	}
}

static const int MAX_SEARCH_LIST = 255;
static CHAR searchList[MAX_SEARCH_LIST + 1];
void CSettings::SetSearchList(CHAR* szList)
{
	size_t n = _tcslen(szList);
	n = min(MAX_SEARCH_LIST, n);
	memcpy(searchList, szList, n);
	searchList[n] = 0;
	Write(STR_APP_REG_VAL_SEARCH_LIST, searchList);
}
DWORD CSettings::InfoTextColor()
{
	return RGB(128, 128, 128);
}
DWORD CSettings::SerachColor()
{
	return RGB(0xA0, 0xA9, 0x3d);
}
DWORD CSettings::CurSerachColor()
{
	return RGB(64, 128, 64);
}
DWORD CSettings::LogListTxtColor()
{
	return RGB(176, 176, 176);
}
DWORD CSettings::LogListBkColor()
{
	return RGB(0, 0, 0);
}
DWORD CSettings::SelectionTxtColor()
{
	return RGB(255, 255, 255);
}
DWORD CSettings::SelectionBkColor(bool haveFocus)
{
	return haveFocus ? RGB(64, 122, 255) : RGB(64, 122, 255);
}
CHAR* CSettings::GetSearchList()
{
	if (!Read(STR_APP_REG_VAL_SEARCH_LIST, searchList, MAX_SEARCH_LIST))
	{
		searchList[0] = 0;
	}
	return searchList;
}



