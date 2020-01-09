#pragma once

#include "RegKeyExt.h"

#define DECL_GET(type, name) public: type Get##name () { return m_##name ;} private: type m_##name
#define DECL_SET(type, name) public: void Set##name ( type ); private: type m_##name
#define DECL_PROP(type, name) public: type Get##name () { return m_##name ;} void Set##name ( type ); private: type m_##name
#define DECL_STR_PROP(type, name, cb) public: type* Get##name () { return m_##name ;} void Set##name ( const type* ); private: type m_##name[cb];

#define MAX_FILTER_ITEM_BUF 300
#define MAX_MODUL_ITEM_BUF MAX_PATH + 30
#define MAX_SRC_ITEM_BUF MAX_PATH + 30
struct DbgSource
{
	char m_szSrc[MAX_PATH];
	int  m_exclSrc;
	void EncodeText(char *szItem) {
		m_szSrc[_countof(m_szSrc) - 1] = 0;
		sprintf_s(szItem, MAX_PATH - 1, "%s,%d", m_szSrc, m_exclSrc);
	}
	bool DecodeText(const char *szItem) {
		ZeroMemory(this, sizeof(*this));
		char *next_token;
		char* token = strtok_s((char*)szItem, ",", &next_token);
		if (token) {
			strncpy_s(m_szSrc, token, _TRUNCATE);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_exclSrc = atoi(token);
		}
		return token != nullptr;
	}
};
struct DbgFilter
{
	char m_szFunc[MAX_FILTER_ITEM_BUF - 20];
	int  m_showFunc;
	int  m_skipFilter;
	void EncodeText(char *szItem) { 
		m_szFunc[_countof(m_szFunc) - 1] = 0;
		sprintf_s(szItem, MAX_FILTER_ITEM_BUF - 1, "%s,%d,%d", m_szFunc, m_showFunc, m_skipFilter);
	}
	bool DecodeText(const char *szItem) { 
		ZeroMemory(this, sizeof(*this));
		char *next_token;
		char* token = strtok_s((char*)szItem, ",", &next_token);
		if (token) {
			strncpy_s(m_szFunc, token, _TRUNCATE);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_showFunc = atoi(token);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_skipFilter = atoi(token);
		}
		return token != nullptr;
	}
};
struct DbgModule
{
	char m_szModul[MAX_PATH];
	void EncodeText(char *szItem) {
		m_szModul[_countof(m_szModul) - 1] = 0;
		sprintf_s(szItem, MAX_MODUL_ITEM_BUF - 1, "%s", m_szModul);
	}
	bool DecodeText(const char *szItem) {
		ZeroMemory(this, sizeof(*this));
		strncpy_s(m_szModul, szItem, _TRUNCATE);
		m_szModul[_countof(m_szModul) - 1] = 0;
		return true;
	}
};
struct DbgSettings
{
	int m_applyFuncFilters;
	int m_applyPathFilters;
	std::vector<DbgFilter> m_arDbgFilter;
	std::vector<DbgModule> m_arDbgModules;
	std::vector<DbgSource> m_arDbgSources;
};

class CSettings : public CRegKeyExt
{
public:
    CSettings();
    ~CSettings();

    void RestoreWindPos(HWND hWnd);
    void SaveWindPos(HWND hWnd);
    void SetConsoleColor(int consoleColor, DWORD& textColor, DWORD& bkColor);
    void SetUIFont(CHAR* lfFaceName, LONG lfWeight, LONG lfHeight);
    bool CheckUIFont(HDC hdc);

    void SetSearchList(CHAR* szList);
    CHAR* GetSearchList();
    DWORD SelectionBkColor(bool haveFocus);
    DWORD SelectionTxtColor();
    DWORD LogListBkColor();
    DWORD LogListTxtColor();
    DWORD SerachColor();
    DWORD CurSerachColor();
    DWORD InfoTextColor();

	DbgSettings m_DbgSettings;
	bool WriteDbgSettings(char* filePath, DbgSettings& dbgSettings);
	bool ReadDbgSettings(char* filePath, DbgSettings& dbgSettings);
	void SetDbgSettings(char* filePath, DbgSettings& dbgSettings);
	char* GetDbgSettingsPath() { return m_DbgSettingsPath; }

    DECL_PROP(int, VertSplitterPos);
    DECL_PROP(int, HorzSplitterPos);
    DECL_PROP(int, StackSplitterPos);
    DECL_PROP(HFONT, Font);
    DECL_PROP(int, FontHeight);
    DECL_PROP(int, FontWidth);
    //DECL_PROP(DWORD, TextColor);
    //DECL_PROP(DWORD, InfoTextColor);
    //DECL_PROP(DWORD, BkColor);
    //DECL_PROP(DWORD, SelColor);
    //DECL_PROP(DWORD, BkSelColor);
    //DECL_PROP(DWORD, SerachColor);
    //DECL_PROP(DWORD, CurSerachColor);
    //DECL_PROP(DWORD, SyncColor);

    DECL_PROP(DWORD, FlowTracesHiden);
    DECL_PROP(DWORD, TreeViewHiden);
    DECL_PROP(DWORD, InfoHiden);
    DECL_PROP(DWORD, ShowElapsedTime);
	DECL_PROP(DWORD, FullSrcPath);
	DECL_PROP(DWORD, FullFnName);

    DECL_PROP(int, ColLineNN);
    DECL_PROP(int, ColNN);
	DECL_PROP(int, ColApp);
	DECL_PROP(int, ColPID);
	DECL_PROP(int, ColThreadNN);
	DECL_PROP(int, ColFunc);
    DECL_PROP(int, ColLine);
    DECL_PROP(int, ColTime);
    DECL_PROP(int, ColCallAddr);
    DECL_PROP(int, FnCallLine);

    DECL_PROP(DWORD, UdpPort);

    DECL_GET(DWORD, FontSize);
    DECL_GET(CHAR*, FontName);
    DECL_GET(DWORD, FontWeight);
    DECL_GET(CHAR*, ResFontName);


private:
    LOGFONT   m_logFont;
	CHAR      m_DbgSettingsPath[MAX_PATH + 1];
    HANDLE    m_resourceFonthandle;

    void AddDefaultFont();
	void InitFont();
	void DeleteFont();
};

extern CSettings gSettings;