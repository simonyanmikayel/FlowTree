#include "stdafx.h"
#include "Helpers.h"
#include "Callback.h"
#include "Dbg.h"
#include "Helpers.h"
#include "Settings.h"
#include "comdef.h"

class Dbg
{
public:
    Dbg(DWORD64 BaseOfDll, char* szModName, MemBuf* pMemBuf, PtrArray<DbgFuncInfo>* pDbgFuncInfo, bool dump);

    bool LoadDataFromPdb();
    bool DumpAllLines();
    void EnumFunctionLines(IDiaSymbol *pFunction);
    void EnumLineNumbers(IDiaEnumLineNumbers *pLines);
	bool SetCurSource(IDiaLineNumber *pLine);
	BYTE GetAttr(char *szFunc);
	bool PathIncluded(char *szPath);
	DWORD64 m_BaseOfDll;
    char* m_srcNameLast;
	char* m_srcShortNameLast;
	DbgFuncInfo m_CurInfo;
    IDiaDataSource *m_pDiaDataSource;
    IDiaSession *m_pDiaSession;
    IDiaSymbol *m_pGlobalSymbol;
    DWORD m_dwMachineType;
    std::wstring m_moduleName;
    MemBuf* m_pMemBuf;
    PtrArray<DbgFuncInfo>* m_pDbgFuncInfo;
    BigArray<DbgLineInfo> *m_pLineArray;
    size_t LineCount;
};

DbgInfo::DbgInfo()
{
    m_pMemBuf = new MemBuf(1024 * 1024 * 1024, 256 * 1024);
    m_pDbgFuncInfo = new PtrArray<DbgFuncInfo>(m_pMemBuf);
}

DbgInfo::~DbgInfo()
{
    delete m_pMemBuf;
    delete m_pDbgFuncInfo;
}

bool DbgInfo::AddInfo(DWORD64 BaseOfDll, char* szModName)
{
    Dbg dbg(BaseOfDll, szModName, m_pMemBuf, m_pDbgFuncInfo, false);
    return dbg.LineCount > 0;
}

char* szOneFunc = "std::_Ptr_base<winrt::XamlTypeInfo::implementation::XamlMetaDataProvider>::_Ptr_base<winrt::XamlTypeInfo::implementation::XamlMetaDataProvider>";

Dbg::Dbg(DWORD64 BaseOfDll, char* szModName, MemBuf* pMemBuf, PtrArray<DbgFuncInfo>* pDbgFuncInfo, bool dump)
{
    ZeroMemory(this, sizeof(*this));
    m_pMemBuf = pMemBuf;
    m_pDbgFuncInfo = pDbgFuncInfo;
    m_BaseOfDll = BaseOfDll;
	DWORD startIndex = pDbgFuncInfo->Count();
    m_dwMachineType = CV_CFL_80386;
	m_moduleName = Helpers::char_to_wstring(szModName);
    m_pLineArray = new BigArray<DbgLineInfo>(1024 * 1024);

    CoInitialize(NULL);
    if (!LoadDataFromPdb())
    {
        stdlog("Failed to load pdb for %s\n", szModName);
        LineCount = 0;
    }
    else if (!DumpAllLines())
    {
        stdlog("Failed to get lines of %s\n", szModName);
        LineCount = 0;
    }
    else if (!m_pDbgFuncInfo->Count())
    {
        stdlog("No line info for %s\n", szModName);
    }

    if (m_pGlobalSymbol) {
        m_pGlobalSymbol->Release();
        m_pGlobalSymbol = NULL;
    }

    if (m_pDiaSession) {
        m_pDiaSession->Release();
        m_pDiaSession = NULL;
    }

	if (m_pDiaDataSource) {
		m_pDiaDataSource->Release();
		m_pDiaDataSource = NULL;
	}

    delete m_pLineArray;
    CoUninitialize();
	if (dump)
	{
		stdlog("Dump of %s (m_BaseOfDll: %p)\n", szModName, m_BaseOfDll);
		DWORD endIndex = pDbgFuncInfo->Count();
		for (DWORD i = startIndex; i < endIndex; i++)
		{
			DbgFuncInfo *p = pDbgFuncInfo->Get(i);
			stdlog("~%d %s %p %p\n", i, p->fnName, p->GetAddrStart(), p->GetAddrEnd());
		}
	}
}

bool Dbg::LoadDataFromPdb()
{
    wchar_t wszExt[MAX_PATH];
    wchar_t *wszSearchPath = L"SRV**\\\\symbols\\symbols"; // Alternate path to search for debug data
    DWORD dwMachType = 0;
    HRESULT hr;
    const wchar_t* szModuleName = m_moduleName.c_str();

    // Obtain access to the provider

    hr = CoCreateInstance(__uuidof(DiaSource),
        NULL,
        CLSCTX_INPROC_SERVER,
        __uuidof(IDiaDataSource),
        (void **)(&m_pDiaDataSource));

    if (FAILED(hr)) {
        stdlog("CoCreateInstance failed - HRESULT = %08X\n", hr);
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        return false;
    }

    _wsplitpath_s(szModuleName, NULL, 0, NULL, 0, NULL, 0, wszExt, MAX_PATH);

    if (!_wcsicmp(wszExt, L".pdb")) {
        // Open and prepare a program database (.pdb) file as a debug data source

        hr = (m_pDiaDataSource)->loadDataFromPdb(szModuleName);

        if (FAILED(hr)) {
            stdlog("loadDataFromPdb failed - HRESULT = %08X\n", hr);

            return false;
        }
    }

    else {
        CCallback callback; // Receives callbacks from the DIA symbol locating procedure,
                            // thus enabling a user interface to report on the progress of
                            // the location attempt. The client application may optionally
                            // provide a reference to its own implementation of this
                            // virtual base class to the IDiaDataSource::loadDataForExe method.
        callback.AddRef();

        // Open and prepare the debug data associated with the executable

        hr = (m_pDiaDataSource)->loadDataForExe(szModuleName, wszSearchPath, &callback);

        if (FAILED(hr)) {
            stdlog("loadDataForExe failed - HRESULT = %08X\n", hr);

            return false;
        }
    }

    // Open a session for querying symbols

    hr = (m_pDiaDataSource)->openSession(&m_pDiaSession);

    if (FAILED(hr)) {
        stdlog("openSession failed - HRESULT = %08X\n", hr);

        return false;
    }

    // Retrieve a reference to the global scope

    hr = (m_pDiaSession)->get_globalScope(&m_pGlobalSymbol);

    if (hr != S_OK) {
        stdlog("get_globalScope failed\n");

        return false;
    }

    // Set Machine type for getting correct register names

    if ((m_pGlobalSymbol)->get_machineType(&dwMachType) == S_OK) {
        switch (dwMachType) {
        case IMAGE_FILE_MACHINE_I386: m_dwMachineType = CV_CFL_80386; break;
        case IMAGE_FILE_MACHINE_IA64: m_dwMachineType = CV_CFL_IA64; break;
        case IMAGE_FILE_MACHINE_AMD64: m_dwMachineType = CV_CFL_AMD64; break;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////
// Dump all the line numbering information contained in the PDB
//  Only function symbols have corresponding line numbering information
bool Dbg::DumpAllLines()
{

    // First retrieve the compilands/modules

    IDiaEnumSymbols *pEnumSymbols;

    if (FAILED(m_pGlobalSymbol->findChildren(SymTagCompiland, NULL, nsNone, &pEnumSymbols))) {
        stdlog("No compiland found\n");
        return false;
    }

    IDiaSymbol *pCompiland;
    ULONG celt = 0;
    bool ret = true;

    while (ret && SUCCEEDED(pEnumSymbols->Next(1, &pCompiland, &celt)) && (celt == 1)) {

        IDiaEnumSymbols *pEnumFunction;

        // For every function symbol defined in the compiland, retrieve and print the line numbering info
        //SymTagFunction SymTagFuncDebugStart
        if (SUCCEEDED(pCompiland->findChildren(SymTagFunction, NULL, nsNone, &pEnumFunction))) {
            IDiaSymbol *pFunction;

            while (SUCCEEDED(pEnumFunction->Next(1, &pFunction, &celt)) && (celt == 1)) {
                ZeroMemory(&m_CurInfo, sizeof(m_CurInfo));
				m_pLineArray->Clear();

                EnumFunctionLines(pFunction);

                if (m_CurInfo.fnName && m_CurInfo.cLine) {
                    m_CurInfo.pLineInfo = m_pMemBuf->New<DbgLineInfo>(m_CurInfo.cLine, false);
                    if (!m_CurInfo.pLineInfo)
                    {
                        stdlog("Not enought memory to load debug info\n");
                        ret = false;
                        break;
                    }
                    memcpy(m_CurInfo.pLineInfo, m_pLineArray->Get(0), m_CurInfo.cLine * sizeof(DbgLineInfo));
                    if (!m_pDbgFuncInfo->Add(m_CurInfo))
                    {
                        stdlog("Not enought memory to load debug info\n");
                        ret = false;
                        break;
                    }
					LineCount += m_CurInfo.cLine;
				}
                pFunction->Release();
            }
            pEnumFunction->Release();
        }
        pCompiland->Release();
    }
    pEnumSymbols->Release();

    return ret;
}


static char* ShortName(char* name, size_t& cb)
{
    char* pDelim1 = strstr(name, "::");
    char* pDelim2 = nullptr, *pShort = nullptr;
    while (pDelim1 && (pDelim2 = strstr(pDelim1 + 2, "::"))) {
        pShort = pDelim1;
        pDelim1 = pDelim2;
    }
    if (pShort) {
        pShort += 2;
        cb -= pShort - name;
    }
    else {
        pShort = name;
    }
    return pShort;
}

#ifdef _DEBUG
char strFnName[MAX_PATH];
#endif
void Dbg::EnumFunctionLines(IDiaSymbol *pFunction)
{
    DWORD dwSymTag;

    if ((pFunction->get_symTag(&dwSymTag) != S_OK) || (dwSymTag != SymTagFunction)) {
        stdlog("ERROR - EnumFunctionLines() dwSymTag != SymTagFunction\n");
        return;
    }

    BSTR bstrName;
    int cbFuncName = 0;

    if (pFunction->get_name(&bstrName) == S_OK) {
        //stdlog("%S \n", bstrName);
        if (wcsstr(bstrName, L"DllMain"))
        {
            SysFreeString(bstrName);
            return;
        }
        cbFuncName = WideCharToMultiByte(CP_ACP, 0, bstrName, -1, NULL, 0, NULL, NULL);
        if (cbFuncName <= 0 || cbFuncName >= MAX_FUNC_NAME_LEN) {
            stdlog("ERROR - WideCharToMultiByte\n");
            SysFreeString(bstrName);
            return;
        }
#ifdef _DEBUG
		WideCharToMultiByte(CP_ACP, 0, bstrName, -1, strFnName, MAX_PATH - 1, NULL, NULL);
		if (strstr(strFnName, szOneFunc))
		{
			szOneFunc = szOneFunc;
		}
#endif
    }
    else {
        stdlog("ERROR - EnumFunctionLines() get_name\n");
        return;
    }

    ULONGLONG ulLength;

    if (pFunction->get_length(&ulLength) != S_OK) {
        stdlog("ERROR - EnumFunctionLines() get_length\n");
        SysFreeString(bstrName);
        return;
    }

    DWORD dwRVA;
    IDiaEnumLineNumbers *pLines;

    if (pFunction->get_relativeVirtualAddress(&dwRVA) == S_OK) {
        //stdlog(" Rva: [%08X] \n", dwRVA);
        if (SUCCEEDED(m_pDiaSession->findLinesByRVA(dwRVA, static_cast<DWORD>(ulLength), &pLines))) {
            EnumLineNumbers(pLines);
            pLines->Release();
        }
    }

    else {
        DWORD dwSect;
        DWORD dwOffset;

        if ((pFunction->get_addressSection(&dwSect) == S_OK) &&
            (pFunction->get_addressOffset(&dwOffset) == S_OK)) {
            //stdlog(" Sect/Offset: [%04X:%08X] \n", dwSect, dwOffset);
            if (SUCCEEDED(m_pDiaSession->findLinesByAddr(dwSect, dwOffset, static_cast<DWORD>(ulLength), &pLines))) {
                EnumLineNumbers(pLines);
                pLines->Release();
            }
        }
        else {
            //stdlog(" Rva/Sect/Offset ???  \n");
        }
    }


    if (m_CurInfo.cLine > 0) {
        m_CurInfo.fnName = m_pMemBuf->New<char>(cbFuncName, false);
        WideCharToMultiByte(CP_ACP, 0, bstrName, -1, m_CurInfo.fnName, cbFuncName, NULL, NULL);
        m_CurInfo.cb_fnName = cbFuncName - 1;
		char* p1 = m_CurInfo.fnName;
		char* p2 = strchr(p1, '<');
		//if (strstr(p1, "stdext::checked_array_iterator<winrt::Windows::UI::Xaml::Markup::XmlnsDefinition *>::operator<"))
		//	p2 = p2;
		if (p2 && cbFuncName > 2) {
			// use bstrName to evaluate function short name
			char *shortFnName = (char*)bstrName;
			size_t cb = 0;
			char *pEnd = p1 + cbFuncName;
			size_t cbBuf = 2 * cbFuncName;
			while (p2 && p2 < pEnd)
			{
				memcpy(shortFnName + cb, p1, (p2 - p1));
				cb += (p2 - p1);
				size_t cBracet = 1;
				p2++;
				for (; p2 < pEnd && cBracet && cb < cbBuf; p2++)
				{
					if (*p2 == '<')
					{
						cBracet++;
					}
					else  if (*p2 == '>')
					{
						cBracet--;
					}
				}
				if (p2 - p1 > 1 && cb < cbBuf - 4)
				{
					shortFnName[cb++] = '<';
					shortFnName[cb++] = cBracet ? '?' : 'T';
					shortFnName[cb++] = '>';
				}
				if (p2 < pEnd)
				{
					p1 = p2;
					p2 = strchr(p1, '<');
					if (!p2)
					{
						p2 = pEnd;
						memcpy(shortFnName + cb, p1, (p2 - p1));
						cb += (p2 - p1);
						break;
					}
				}
			}
            if (cb <= 1)
                cb = 2;
            shortFnName[cb] = 0;
            char* pShort = ShortName(shortFnName, cb);
			m_CurInfo.cb_shortFnName = DWORD(cb - 1);
			m_CurInfo.shortFnName = m_pMemBuf->New<char>(cb, false);
			memcpy(m_CurInfo.shortFnName, pShort, m_CurInfo.cb_shortFnName);
			m_CurInfo.shortFnName[m_CurInfo.cb_shortFnName] = 0;
			p2 = p2;
        } 
        else {
            size_t cb = m_CurInfo.cb_fnName;
            char* pShort = ShortName(m_CurInfo.fnName, cb);
            m_CurInfo.shortFnName = pShort;
            m_CurInfo.cb_shortFnName = (DWORD)cb;
        }

		m_CurInfo.attr = GetAttr(m_CurInfo.fnName);
        if (!m_CurInfo.pathIncluded) {
            if (!m_CurInfo.attr && !(m_CurInfo.attr & LOG_ATTR_SHOW_FUNC)) {
                m_CurInfo.cLine = 0;
            }
        }
    }

    SysFreeString(bstrName);
}

BYTE Dbg::GetAttr(char *szFunc)
{
	if (gSettings.m_DbgSettings.m_applyFuncFilters)
	{
		std::vector<DbgFilter>& arDbgFilter = gSettings.m_DbgSettings.m_arDbgFilter;
		size_t c = arDbgFilter.size();
		for (size_t i = 0; i < c; i++)
		{
			if (!arDbgFilter[i].m_skipFilter)
			{
				if (strstr(szFunc, arDbgFilter[i].m_szFunc))
				{
					if (arDbgFilter[i].m_showFunc == 0)
                        return LOG_ATTR_SKIP_FUNC;
                    else
                        return LOG_ATTR_SHOW_FUNC;
                }
			}
		}
	}
	return 0;
}

bool Dbg::PathIncluded(char *szPath)
{
	if (gSettings.m_DbgSettings.m_applyPathFilters)
	{
		std::vector<DbgSource>& arDbgSources = gSettings.m_DbgSettings.m_arDbgSources;
		size_t c = arDbgSources.size();
		if (c > 0)
		{
			bool haveIncl = false;
			bool included = false;
			for (size_t i = 0; i < c; i++)
			{
				haveIncl = haveIncl || !(arDbgSources[i].m_exclSrc);
				if (StrStrIA(szPath, arDbgSources[i].m_szSrc))
				{
					if (arDbgSources[i].m_exclSrc)
						return false;
					else
						included = true;
				}
			}
			return included || !haveIncl;
		}
	}
	return true;
}

bool Dbg::SetCurSource(IDiaLineNumber *pLine)
{
	bool ret = false;
	BSTR bstrSourceName = nullptr;
	IDiaSourceFile *pSource = nullptr;

	if (pLine->get_sourceFile(&pSource) == S_OK)
	{
		if (pSource->get_fileName(&bstrSourceName) == S_OK)
		{
			char strBuf[4 * MAX_PATH];
			strBuf[0] = 0;
			int cb = WideCharToMultiByte(CP_ACP, 0, bstrSourceName, -1, NULL, 0, NULL, NULL);
			if (cb > 0 && cb < _countof(strBuf))
				WideCharToMultiByte(CP_ACP, 0, bstrSourceName, -1, strBuf, cb, NULL, NULL);
			if (strBuf[0] != 0)
			{
				if (!m_srcNameLast || 0 != strcmp(strBuf, m_srcNameLast))
				{
					m_srcNameLast = m_pMemBuf->New<char>(cb, false);
					memcpy(m_srcNameLast, strBuf, cb);
					m_srcShortNameLast = strrchr(m_srcNameLast, '\\');
					if (!m_srcShortNameLast)
						m_srcShortNameLast = m_srcNameLast;
					else
						m_srcShortNameLast++;
				}
				ret = true;
			}
		}
		else {
			stdlog("ERROR get_fileName\n");
		}
	}
	else {
		stdlog("ERROR get_sourceFile\n");
	}
	if (bstrSourceName)
		SysFreeString(bstrSourceName);
	if (pSource)
		pSource->Release();
	return ret;
}

void Dbg::EnumLineNumbers(IDiaEnumLineNumbers *pLines)
{
    IDiaLineNumber *pLine = nullptr;
    DWORD celt;
    DWORD dwRVA;
    DWORD dwSeg;
    DWORD dwOffset;
    DWORD dwLinenum;
    DWORD dwSrcId, dwSrcIdLast = -1;
    DWORD dwLength;

	while (SUCCEEDED(pLines->Next(1, &pLine, &celt)) && (celt == 1)) 
	{
        if (
            (pLine->get_relativeVirtualAddress(&dwRVA) == S_OK)
            && (pLine->get_addressSection(&dwSeg) == S_OK)
            && (pLine->get_addressOffset(&dwOffset) == S_OK)
            && (pLine->get_lineNumber(&dwLinenum) == S_OK)
            && (pLine->get_sourceFileId(&dwSrcId) == S_OK)
            && (pLine->get_length(&dwLength) == S_OK)
            )
        {
            //stdlog("\tline %u at [%08X][%04X:%08X], len = 0x%X\n", dwLinenum, dwRVA, dwSeg, dwOffset, dwLength);
            if (dwSrcId != dwSrcIdLast) 
			{
				// parse source only once for a dwSrcId
                dwSrcIdLast = dwSrcId;
				if (!SetCurSource(pLine))
				{
					m_CurInfo.cLine = 0;
					break;
				}
            }

            DbgLineInfo* pDbgLineInfo = m_pLineArray->Add();
            if (pDbgLineInfo)
            {
				pDbgLineInfo->dwSrcId = dwSrcId;
                pDbgLineInfo->line = dwLinenum;
                pDbgLineInfo->addr = dwRVA + m_BaseOfDll;
                pDbgLineInfo->dwLength = dwLength;
				pDbgLineInfo->srcName = m_srcNameLast;
				pDbgLineInfo->srcShortName = m_srcShortNameLast;
				if (m_CurInfo.cLine > 1) {
                    if ((pDbgLineInfo-1)->addr >= pDbgLineInfo->addr)
                        stdlog("ERROR address is not incremented\n");
                    if ((pDbgLineInfo - 1)->addr + (pDbgLineInfo - 1)->dwLength > pDbgLineInfo->addr)
                        stdlog("ERROR address overlaped\n");
                }
				m_CurInfo.cLine++;
            }
            else
            {
                stdlog("ERROR m_pLineArray->Add\n");
				m_CurInfo.cLine = 0;
				break;
            }
        }
		pLine->Release();
		pLine = nullptr;
	}

	dwSrcIdLast = -1;
	bool pathIncluded = false;
	for (DWORD i = 0; i < m_CurInfo.cLine; i++)
	{
		if (m_pLineArray->Get(i)->dwSrcId != dwSrcIdLast)
		{
			dwSrcIdLast = m_pLineArray->Get(i)->dwSrcId;
			pathIncluded = pathIncluded || PathIncluded(m_pLineArray->Get(i)->srcName);
		}
	}

    m_CurInfo.pathIncluded = pathIncluded;

	if (pLine)
		pLine->Release();
}