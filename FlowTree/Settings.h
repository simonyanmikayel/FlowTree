#pragma once

#include "RegKeyExt.h"

struct RegExString
{
	RegExString(CRegKeyExt* pReg, LPCTSTR key, int max_buf, LPTSTR defVal = nullptr) {
		pRegKey = pReg;
		max_buffer = max_buf;
		buffer = new CHAR[max_buffer + 1];
		regKey = key;

		if (!pRegKey->Read(regKey, buffer, max_buffer, defVal))
		{
			buffer[0] = 0;
		}
	}
	~RegExString() {
		delete[] buffer;
	}
	void set(const CHAR* sz, size_t n) {
		n = min((size_t)max_buffer, n);
		memcpy(buffer, sz, n);
		buffer[n] = 0;
		pRegKey->Write(regKey, buffer);
	}
	void set(const CHAR* sz) {
		size_t n = strlen(sz);
		set(sz, n);
	}
	const CHAR* get() {
		return buffer;
	}
private:
	CHAR* buffer;
	LPCTSTR regKey;
	int max_buffer;
	CRegKeyExt* pRegKey;
};

template <typename T> struct RegExNum
{
	RegExNum(CRegKeyExt* pReg, LPCTSTR key, T def) {
		pRegKey = pReg;
		regKey = key;
		pRegKey->Read(regKey, val, (DWORD)def);
		//if (dbgKey && !strcmp(regKey, dbgKey))
		//	def = def;
	}
	void set(T i) {
		//if (dbgKey && !strcmp(regKey, dbgKey))
		//	i = i;
		val = (DWORD)i;
		pRegKey->Write(regKey, i);
	}
	const T get() {
		//if (dbgKey && !strcmp(regKey, dbgKey))
		//	val = val;
		return (T)val;
	}

private:
	DWORD val;
	LPCTSTR regKey = 0;
	//LPCTSTR dbgKey = "CombainLoop";
	CRegKeyExt* pRegKey = 0;
};

template <typename T> struct ReadOnly
{
	friend class CSettings;
	ReadOnly(T def = 0) {
		val = def;
	}
	const T get() {
		return val;
	}
private:
	ReadOnly<T>& operator = (T v) {
		val = v;
		return *this;
	}
	operator T() {
		return val;
	}
	T val;
};

#define PROP_STR(var) RegExString m_##var; const CHAR* var() {return m_##var.get();} void var(const CHAR* val){m_##var.set(val);}
#define PROP_NUM(type, var) RegExNum<type> m_##var; type var(){return m_##var.get();} void var( type val){m_##var.set(val);}
#define PROP_GET(type, var) ReadOnly<type> m_##var; type var(){return m_##var.get();}

#define MAX_FILTER_ITEM_BUF 300
#define MAX_MODUL_ITEM_BUF MAX_PATH + 30
#define MAX_SRC_ITEM_BUF MAX_PATH + 30
struct DbgSource
{
	char m_szSrc[MAX_FILTER_ITEM_BUF - 20];
	int  m_showSrc = 0;
	int  m_Priority = 0;
	int  m_skipSrc = 0;
	void EncodeText(char *szItem) {
		m_szSrc[_countof(m_szSrc) - 1] = 0;
		sprintf_s(szItem, MAX_FILTER_ITEM_BUF - 1, "%s,%d,%d,%d", m_szSrc, m_showSrc, m_Priority, m_skipSrc);
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
			m_showSrc = atoi(token);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_Priority = atoi(token);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_skipSrc = atoi(token);
		}
		return token != nullptr;
	}
};
struct DbgFilter
{
	char m_szFunc[MAX_FILTER_ITEM_BUF - 20];
	int  m_showFunc = 0;
	int  m_Priority = 0;
	int  m_skipFilter = 0;
	void EncodeText(char *szItem) { 
		m_szFunc[_countof(m_szFunc) - 1] = 0;
		sprintf_s(szItem, MAX_FILTER_ITEM_BUF - 1, "%s,%d,%d,%d", m_szFunc, m_showFunc, m_Priority, m_skipFilter);
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
			m_Priority = atoi(token);
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
	char m_szModul[MAX_PATH] = {0};
	int  m_skipModule = 0;
	void EncodeText(char *szItem) {
		m_szModul[_countof(m_szModul) - 1] = 0;
		sprintf_s(szItem, MAX_MODUL_ITEM_BUF - 1, "%s,%d", m_szModul, m_skipModule);
	}
	bool DecodeText(const char *szItem) {
		ZeroMemory(this, sizeof(*this));
		char* next_token;
		char* token = strtok_s((char*)szItem, ",", &next_token);
		if (token) {
			strncpy_s(m_szModul, token, _TRUNCATE);
			token = strtok_s(NULL, ",", &next_token);
		}
		if (token) {
			m_skipModule = atoi(token);
		}
		return token != nullptr;
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

enum class IDE_TYPE {
	VS,
	QT,
	CLion
};

class CSettings : public CRegKeyExt
{
public:
    CSettings();
    ~CSettings();

    void RestoreWindPos(HWND hWnd);
    void SaveWindPos(HWND hWnd);
    void SetConsoleColor(int MsgType, DWORD& textColor, DWORD& bkColor);
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
	bool ReLoadDbgSettings();
	void SetDbgSettings(char* filePath, DbgSettings& dbgSettings);
	char* DbgSettingsPath() { return m_DbgSettingsPath; }
	void SetQtCreatorPath(const char* filePath);
	void SetCLionPath(const char* filePath);
	char* QtCreatorPath() { return m_QtCreatorPath; }
	char* CLionPath() { return m_CLionPath; }

	static DWORD CSettings::SelectionBkColor() { return RGB(64, 64, 64); }
	static DWORD CSettings::InfoTextColorNative() { return RGB(0, 0, 0); }
	static DWORD CSettings::LogListInfoBkColor() { return RGB(240, 240, 240); }
	static DWORD CSettings::CurSelectionTxtColor() { return RGB(255, 255, 255); }
	static DWORD CSettings::CurSelectionBkColor() { return RGB(64, 122, 255); }

    PROP_NUM(int, VertSplitterPos);
    PROP_NUM(int, HorzSplitterPos);
    PROP_NUM(int, StackSplitterPos);
	PROP_GET(HFONT, Font);
	PROP_GET(DWORD, fontSize);
	PROP_GET(int, FontHeight);
	PROP_GET(int, FontWidth);

    PROP_NUM(DWORD, FlowTracesHiden);
    PROP_NUM(DWORD, TreeViewHiden);
    PROP_NUM(DWORD, InfoHiden);
    PROP_NUM(DWORD, ShowElapsedTime);
	PROP_NUM(DWORD, FullSrcPath);
	PROP_NUM(DWORD, FullFnName);
	PROP_NUM(DWORD, DissableBufferization);
	PROP_NUM(DWORD, UseTcpForLog);
	PROP_NUM(DWORD, IdeType);

    PROP_NUM(int, ColNN);
	PROP_NUM(int, ColApp);
	PROP_NUM(int, ColPID);
	PROP_NUM(int, ColThreadNN);
	PROP_NUM(int, ColFunc);
    PROP_NUM(int, ColLine);
    PROP_NUM(int, ColTime);
    PROP_NUM(int, ColCallAddr);
    PROP_NUM(int, FnCallLine);

	PROP_GET(DWORD, FontSize);
	PROP_GET(CHAR*, FontName);
	PROP_GET(DWORD, FontWeight);
	PROP_GET(CHAR*, ResFontName);


private:
    LOGFONT   m_logFont;
	CHAR      m_DbgSettingsPath[MAX_PATH + 1];
	CHAR      m_QtCreatorPath[MAX_PATH + 1];
	CHAR      m_CLionPath[MAX_PATH + 1];
	HANDLE    m_resourceFonthandle;

    void AddDefaultFont();
	void InitFont();
	void DeleteFont();
};

extern CSettings gSettings;