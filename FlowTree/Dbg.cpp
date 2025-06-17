#include "stdafx.h"
#include "Buffer.h"
#include "Helpers.h"
#include "Callback.h"
#include "Dbg.h"
#include "Helpers.h"
#include "Settings.h"
#include "comdef.h"

char fnName[MAX_FUNC_NAME_LEN + 1] = {0};
char srcNameLast[MAX_SRC_NAME_LEN + 1] = { 0 };
DWORD  srcNameOffset = 0;
DbgLoadStatus gDbgLoadStatus;
static int PathIncluded(char* szPath);
static BYTE GetAttr(char* szFunc);

class Dbg
{
public:
    Dbg();
    bool Init(ModuleData* moduleData, const char* szModName);
    bool LoadDataFromPdb(const char* szModName);
    bool DumpAllLines();
    void EnumFunctionLines(IDiaSymbol *pFunction);
    void EnumLineNumbers(IDiaEnumLineNumbers *pLines);
	bool SetCurSource(IDiaLineNumber *pLine);

    ModuleData* m_moduleData;
    size_t LineCount;
    DbgFuncInfo m_CurInfo;
    int cb_shortSrcNameName;
    IDiaDataSource *m_pDiaDataSource;
    IDiaSession *m_pDiaSession;
    IDiaSymbol *m_pGlobalSymbol;
    DWORD m_dwMachineType;
    BigArray<DbgLineInfo>* m_pLineArray;
};


DbgInfo::DbgInfo(const char* _appPath) {
    strcpy_s(appPath, _appPath);

    std::stringstream ss;
    ss << gSettings.m_DbgSettings.m_applyFuncFilters;
    ss << gSettings.m_DbgSettings.m_applyPathFilters;
    for (auto a : gSettings.m_DbgSettings.m_arDbgFilter) {
        ss << a.m_szFunc;
        ss << a.m_showFunc;
        ss << a.m_Priority;
        ss << a.m_skipFilter;
    }
    //for (auto a : gSettings.m_DbgSettings.m_arDbgModules)
    //    ss << a.m_szModul;
    for (auto a : gSettings.m_DbgSettings.m_arDbgSources) {
        ss << a.m_szSrc;
        ss << a.m_Priority;
        ss << a.m_showSrc;
        ss << a.m_skipSrc;
    }
    crcDbgSettings = Helpers::crc32(ss.str());
}

DbgInfo::~DbgInfo()
{
    for (auto a : arModuleData)
    {
        delete a;
    }
}

inline DbgLineInfo* ModuleData::GetFirstLineInfo(DWORD funcId)
{
    return (DbgLineInfo*)data.GetVal(GetDbgFuncInfo(funcId)->lineInfoOffset);
}

ModuleData::ModuleData(const CmdModule* module)
{
    startAddr = module->startAddr;
    endAddr = module->endAddr;
    strcpy_s(szModName, module->szModName);

}

ModuleData::~ModuleData()
{
    if (!ready) {
        std::string strModName{ szModName };
        std::string strDbgIfoFile = strModName + DbgInfExtention;
        std::string strDbgDatFile = strModName + DbgDatExtention;
        DeleteFileA(strDbgIfoFile.c_str());
        DeleteFileA(strDbgDatFile.c_str());
    }
}

DWORD64 ModuleData::GetAddrStart(DWORD funcId) {
    DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
    if (!pDbgFuncInfo || !pDbgFuncInfo->cLine)
        return 0;
    return pDbgFuncInfo->addrStart;
    //DbgLineInfo* pFirstLineInfo = GetFirstLineInfo(funcId);
    //return pFirstLineInfo[0].addr;
}

DWORD64 ModuleData::GetAddrEnd(DWORD funcId) {
    DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
    if (!pDbgFuncInfo || !pDbgFuncInfo->cLine)
        return 0;
    return pDbgFuncInfo->addrEnd;
    //DbgLineInfo* pFirstLineInfo = GetFirstLineInfo(funcId);
    //return pFirstLineInfo[pDbgFuncInfo->cLine - 1].addr + pFirstLineInfo[pDbgFuncInfo->cLine - 1].dwLength;
}

bool ModuleData::GetFuncAddresses(DWORD funcId, FUNC_ADDR& fa)
{
    DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
    if (!pDbgFuncInfo || !pDbgFuncInfo->cLine)
        return false;
    fa.addrStart = pDbgFuncInfo->addrStart;
    fa.addrEnd = pDbgFuncInfo->addrEnd;
    fa.attr = pDbgFuncInfo->attr;
    return true;
}

DbgLineInfo* ModuleData::GetLineInfo(DWORD funcId, DWORD64 funcAddr)
{
    DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
    if (!pDbgFuncInfo)
        return nullptr;
    DbgLineInfo* pFirstLineInfo = GetFirstLineInfo(funcId);
    DbgLineInfo* p = pFirstLineInfo;
    DbgLineInfo* pRet = pFirstLineInfo;
    DWORD64 min_diff = -1;
    for (WORD i = 0; i < pDbgFuncInfo->cLine; i++) {
        DWORD64 lineAddr = p->addr + startAddr;
        DWORD64 diff = (funcAddr > lineAddr) ? (funcAddr - lineAddr) : (lineAddr - funcAddr);
        if (diff < min_diff && p->line != 0xF00F00)
        {
            min_diff = diff;
            pRet = p;
        }
        p++;
    }
    //for (WORD i = 0; !(p->addr <= address && p->addr + p->dwLength >= address) && i < cLine - 1; i++)
    //	p++;
    return pRet;
}

// DWORD ModuleData::cbFnName(DWORD funcId, bool forceFullName)
// {
//     DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
//     if (!pDbgFuncInfo)
//         return 0;
//     if (forceFullName || gSettings.FullFnName())
//         return data.GetSize(pDbgFuncInfo->fnNameOffset) - 1;
//     else
//         return pDbgFuncInfo->shortFnNameSize;
// }

char* ModuleData::fnName(DWORD funcId, bool forceFullName)
{
    DbgFuncInfo* pDbgFuncInfo = GetDbgFuncInfo(funcId);
    if (!pDbgFuncInfo)
        return "?";
    if (forceFullName || gSettings.FullFnName())
        return data.GetVal(pDbgFuncInfo->fnNameOffset);
    else
        return data.GetValEnd(pDbgFuncInfo->fnNameOffset) - pDbgFuncInfo->shortFnNameSize - 1;
}

char* ModuleData::getFuncSrc(DWORD funcId, DWORD64 funcAddr, bool fullPath)
{
    char* src = "";
    DbgLineInfo* pLineInfo = GetLineInfo(funcId, funcAddr);
    if (pLineInfo) {
        src = data.GetVal(pLineInfo->srcNameOffset);
        if (!fullPath) {
            char* srcShortName = strrchr(src, '\\');
            if (srcShortName)
                src = srcShortName++;
        }
    }
    return src;

}

bool DbgInfo::DbgFileMapHeaderValid(const WIN32_FIND_DATAA& moduleFileData, const DbgFileMapHeader& hdr)
{
    if (hdr.version != DbgFileMapHeaderVersion)
        return false;
    if (hdr.magic != DbgFileMapHeaderMagic)
        return false;
    if (0 != memcmp(&hdr.ftLastWriteTime, &moduleFileData.ftLastWriteTime, sizeof(FILETIME)))
        return false;    
    return true;
}

void DbgInfo::CheckDbgSettings(ModuleData* moduleData)
{
    DbgFileMapHeader *hdrInfo, *hdrData;
    hdrInfo = (DbgFileMapHeader*)moduleData->info.m_ptr;
    hdrData = (DbgFileMapHeader*)moduleData->data.m_ptr;
    if (hdrInfo->crcDbgSettings != crcDbgSettings || hdrData->crcDbgSettings != crcDbgSettings)
    {
        int pathAttr = 0;
        srcNameLast[0] = 0;
        DWORD funcCount = (DWORD)moduleData->info.Count();
        for (DWORD funcId = 0; funcId < funcCount; funcId++)
        {
            if (gDbgLoadStatus.cSettingsChecked++ % 1000 == 0)
                Helpers::UpdateDbgLoadStatus();
            DbgFuncInfo* pDbgFuncInfo = moduleData->GetDbgFuncInfo(funcId);
            char* fnName = moduleData->data.GetVal(pDbgFuncInfo->fnNameOffset);
            DbgLineInfo* pLineInfo = moduleData->GetFirstLineInfo(funcId);
            char* src = moduleData->data.GetVal(pLineInfo->srcNameOffset);
            if (0 != strcmp(src, srcNameLast))
            {
                strncpy_s(srcNameLast, src, MAX_SRC_NAME_LEN);
                pathAttr = PathIncluded(srcNameLast);
            }
            pDbgFuncInfo->attr = GetAttr(fnName);
            if (pathAttr) {
                if (!pDbgFuncInfo->attr) {
                    pDbgFuncInfo->attr = pathAttr;
                }
            }
        }
        hdrInfo->crcDbgSettings = crcDbgSettings;
        hdrData->crcDbgSettings = crcDbgSettings;
    }
}

bool DbgInfo::OpenFileMaps(ModuleData* moduleData, const char* szModName, bool openExisting)
{
    std::string strModName{ szModName };
    std::string strDbgIfoFile = strModName + DbgInfExtention;
    std::string strDbgDatFile = strModName + DbgDatExtention;
    DWORD dwCreationDisposition = openExisting ? OPEN_EXISTING : CREATE_ALWAYS;
    if (!moduleData->info.Create(strDbgIfoFile.c_str(), true, dwCreationDisposition))
    {
        if (!openExisting)
            stdlog("Failed to create %s\n", strDbgIfoFile.c_str());
        goto err;
    }
    if (!moduleData->data.Create(strDbgDatFile.c_str(), true, dwCreationDisposition))
    {
        if (!openExisting)
            stdlog("Failed to create %s\n", strDbgDatFile.c_str());
        goto err;
    }
    WIN32_FIND_DATAA moduleFileData;
    HANDLE hFind = FindFirstFileA(szModName, &moduleFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        stdlog("Error finding file %s: %lu\n", szModName, GetLastError());
        goto err;
    }
    FindClose(hFind);
    if (openExisting) {
        DbgFileMapHeader hdr;
        if (!moduleData->info.Read(&hdr, sizeof(hdr)))
            goto err;
        if (!DbgFileMapHeaderValid(moduleFileData, hdr))
            goto err;
        if (!moduleData->data.Read(&hdr, sizeof(hdr)))
            goto err;
        if (!DbgFileMapHeaderValid(moduleFileData, hdr))
            goto err;
        if (!moduleData->info.Map() || !moduleData->data.Map())
        {
            stdlog("Failed to open map for existing files for %s\n", szModName);
            goto err;
        }
        moduleData->info.SetHeaderSize(sizeof(hdr));
        CheckDbgSettings(moduleData);
    }
    else {
        DbgFileMapHeader hdr;
        hdr.ftLastWriteTime = moduleFileData.ftLastWriteTime;
        hdr.crcDbgSettings = 0;
        if (!moduleData->info.Write(&hdr, sizeof(hdr)))
            goto err;
        if (!moduleData->data.Write(&hdr, sizeof(hdr)))
            goto err;
        moduleData->info.SetHeaderSize(sizeof(hdr));
    }
    return true;
err:
    return false;
}

bool DbgInfo::AddModule(const CmdModule* cmdModule)
{
    Dbg dbg;
    ModuleData* moduleData = new ModuleData(cmdModule);

    if (OpenFileMaps(moduleData, cmdModule->szModName, true))
    {
        goto ok;
    }
    if (!OpenFileMaps(moduleData, cmdModule->szModName, false))
    {
        stdlog("Failed to create FileMaps for %s\n", cmdModule->szModName);
        goto err;
    }
    if (!dbg.Init(moduleData, cmdModule->szModName))
    {
        stdlog("Failed to init dbg for %\n", cmdModule->szModName);
        goto err;
    }
    if (!moduleData->info.Map() || !moduleData->data.Map())
    {
        stdlog("Failed to map dbg files for %s\n", cmdModule->szModName);
        goto err;
    }
    DbgFuncInfo* first = &moduleData->info[0];
    DbgFuncInfo* last = &moduleData->info[moduleData->info.Count() - 1];
    std::sort(first, last, [](const DbgFuncInfo& a, const DbgFuncInfo& b) {
        return a.addrStart < b.addrStart;
    });

    DbgFileMapHeader* hdr;
    hdr = (DbgFileMapHeader*)moduleData->info.m_ptr;
    hdr->magic = DbgFileMapHeaderMagic;
    hdr = (DbgFileMapHeader*)moduleData->data.m_ptr;
    hdr->magic = DbgFileMapHeaderMagic;
    moduleData->info.UnMap(true);
    moduleData->data.UnMap(true);
    if (!OpenFileMaps(moduleData, cmdModule->szModName, true))
    {
        stdlog("Failed to reopen file maps for %s\n", cmdModule->szModName);
        goto err;
    }

ok:
    arModuleData.push_back(moduleData);
    moduleData->ready = true;
    return true;
err:
    if (moduleData) {
        delete moduleData;
    }
    return false;
}

Dbg::Dbg()
{
    ZeroMemory(this, sizeof(*this));
}

bool Dbg::Init(ModuleData* _moduleData, const char* szModName)
{
    m_moduleData = _moduleData;

    m_dwMachineType = CV_CFL_80386;
    m_pLineArray = new BigArray<DbgLineInfo>(1024 * 1024);

    CoInitialize(NULL);
    if (!LoadDataFromPdb(szModName))
    {
        stdlog("Failed to load pdb for %s\n", szModName);
        return false;
    }
    else if (!DumpAllLines())
    {
        stdlog("Failed to get lines of %s\n", szModName);
        LineCount = 0;
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
    return LineCount != 0;
}

bool Dbg::LoadDataFromPdb(const char* szModName)
{
    std::wstring str_moduleName = Helpers::char_to_wstring(szModName);
    const wchar_t* szModuleName = str_moduleName.c_str();
    wchar_t wszExt[MAX_PATH];
    wchar_t *wszSearchPath = L"SRV**\\\\symbols\\symbols"; // Alternate path to search for debug data
    DWORD dwMachType = 0;
    HRESULT hr;

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
    srcNameLast[0] = 0;

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
                if (gDbgLoadStatus.stop) {
                    ret = false;
                    break;
                }
                if (gDbgLoadStatus.cFunctionsLoaded++ % 100 == 0)
                    Helpers::UpdateDbgLoadStatus();
                if (m_CurInfo.fnNameOffset && m_CurInfo.cLine) {
                    m_CurInfo.lineInfoOffset = m_moduleData->data.Add(m_pLineArray->Get(0), m_CurInfo.cLine * sizeof(DbgLineInfo));
                    m_CurInfo.addrStart = m_pLineArray->Get(0)->addr;
                    m_CurInfo.addrEnd = m_pLineArray->Get(m_pLineArray->Count() - 1)->addr + m_pLineArray->Get(m_pLineArray->Count() - 1)->dwLength;
                    m_moduleData->info.Add(&m_CurInfo);
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


static char* ShortName(char* name, DWORD& cb)
{
    char* pDelim1 = strstr(name, "::");
    char* pDelim2 = nullptr, *pShort = nullptr;
    while (pDelim1 && (pDelim2 = strstr(pDelim1 + 2, "::"))) {
        pShort = pDelim1;
        pDelim1 = pDelim2;
    }
    if (pShort) {
        pShort += 2;
        cb -= (DWORD)(pShort - name);
    }
    else {
        pShort = name;
    }
    return pShort;
}

void Dbg::EnumFunctionLines(IDiaSymbol *pFunction)
{
    DWORD dwSymTag;

    if ((pFunction->get_symTag(&dwSymTag) != S_OK) || (dwSymTag != SymTagFunction)) {
        //stdlog("ERROR - EnumFunctionLines() dwSymTag != SymTagFunction\n");
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
        cbFuncName = WideCharToMultiByte(CP_ACP, 0, bstrName, -1, fnName, _countof(fnName) - 1, NULL, NULL);
        if (cbFuncName <= 0 || cbFuncName >= _countof(fnName)) {
            //stdlog("ERROR - WideCharToMultiByte\n");
            SysFreeString(bstrName);
            return;
        }
#ifdef _DEBUG
  //      wchar_t* szOneFunc = L"std::_Ptr_base<winrt::XamlTypeInfo::implementation::XamlMetaDataProvider>::_Ptr_base<winrt::XamlTypeInfo::implementation::XamlMetaDataProvider>";
		//if (wcsstr(bstrName, szOneFunc))
		//{
		//	szOneFunc = szOneFunc;
		//}
#endif
    }
    else {
        //stdlog("ERROR - EnumFunctionLines() get_name\n");
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
        m_CurInfo.fnNameOffset = m_moduleData->data.Add(fnName, cbFuncName);
		char* p1 = fnName;
		char* p2 = strchr(p1, '<');
		//if (strstr(p1, "stdext::checked_array_iterator<winrt::Windows::UI::Xaml::Markup::XmlnsDefinition *>::operator<"))
		//	p2 = p2;
		if (p2 && cbFuncName > 2) {
			// use bstrName as a temp buffer to evaluate function short name
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
            m_CurInfo.shortFnNameSize = cbFuncName - 1;
            ShortName(shortFnName, m_CurInfo.shortFnNameSize);
			p2 = p2;
        } 
        else {
            m_CurInfo.shortFnNameSize = cbFuncName - 1;
            ShortName(fnName, m_CurInfo.shortFnNameSize);
        }

        //if (strstr(m_CurInfo.fnName, "dynamic ")) {
        //    stdlog("fn: %s\n", m_CurInfo.fnName);
        //}
    }

    SysFreeString(bstrName);
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
			char strBuf[MAX_SRC_NAME_LEN];
			strBuf[0] = 0;
			int cb = WideCharToMultiByte(CP_ACP, 0, bstrSourceName, -1, strBuf, _countof(strBuf) - 1, NULL, NULL);				
			if (cb > 0 && cb < _countof(strBuf))
			{
				if (0 != strcmp(strBuf, srcNameLast))
				{
					memcpy(srcNameLast, strBuf, cb);
                    srcNameOffset = m_moduleData->data.Add(srcNameLast, cb );
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
                pDbgLineInfo->addr = dwRVA;
                pDbgLineInfo->dwLength = dwLength;
				pDbgLineInfo->srcNameOffset = srcNameOffset;
				if (m_CurInfo.cLine > 1) {
                    //if ((pDbgLineInfo-1)->addr >= pDbgLineInfo->addr)
                    //    stdlog("ERROR address is not incremented\n");
                    //if ((pDbgLineInfo - 1)->addr + (pDbgLineInfo - 1)->dwLength > pDbgLineInfo->addr)
                    //    stdlog("ERROR address overlaped\n");
                }
				m_CurInfo.cLine++;
            }
            else
            {
                stdlog("ERROR too many lines in function\n");
				m_CurInfo.cLine = 0;
				break;
            }
        }
		pLine->Release();
		pLine = nullptr;
	}

	if (pLine)
		pLine->Release();
}

static int PathIncluded(char* szPath)
{
    int attr = 0;
    if (gSettings.m_DbgSettings.m_applyPathFilters)
    {
        std::vector<DbgSource>& arDbgSources = gSettings.m_DbgSettings.m_arDbgSources;
        size_t c = arDbgSources.size();
        if (c > 0)
        {
            char* appliedSrc = nullptr;
            int appliedPriority = 0;
            for (size_t i = 0; i < c; i++)
            {
                if (!arDbgSources[i].m_skipSrc && StrStrIA(szPath, arDbgSources[i].m_szSrc))
                {
                    if (!appliedSrc || !StrStrIA(szPath, appliedSrc) || arDbgSources[i].m_Priority >= appliedPriority)
                    {
                        if (arDbgSources[i].m_showSrc == 0)
                            attr = LOG_ATTR_SKIP_FUNC;
                        else if (arDbgSources[i].m_showSrc == 1)
                            attr = LOG_ATTR_SHOW_FUNC;
                        else
                            attr = LOG_ATTR_HIDE_FUNC;
                        appliedSrc = arDbgSources[i].m_szSrc;
                        appliedPriority = arDbgSources[i].m_Priority;
                    }
                }
            }
        }
    }
    return attr;
}

BYTE GetAttr(char* szFunc)
{
    int attr = 0;
    if (gSettings.m_DbgSettings.m_applyFuncFilters)
    {
        std::vector<DbgFilter>& arDbgFilter = gSettings.m_DbgSettings.m_arDbgFilter;
        size_t c = arDbgFilter.size();
        int appliedPriority = -1;
        for (size_t i = 0; i < c; i++)
        {
            if (!arDbgFilter[i].m_skipFilter)
            {
                bool contains = false;
                if (arDbgFilter[i].m_szFunc[0] == '=')
                    contains = (0 == strcmp(szFunc, arDbgFilter[i].m_szFunc + 1));
                else if (arDbgFilter[i].m_szFunc[0] == '<')
                    contains = (0 == strncmp(szFunc, arDbgFilter[i].m_szFunc + 1, strlen(arDbgFilter[i].m_szFunc + 1)));
                else if (arDbgFilter[i].m_szFunc[0] == '>') {
                    int pos = (int)strlen(szFunc) - (int)strlen(arDbgFilter[i].m_szFunc + 1);
                    if (pos >= 0)
                        contains = (0 == strcmp(szFunc + pos, arDbgFilter[i].m_szFunc + 1));
                }
                else
                    contains = strstr(szFunc, arDbgFilter[i].m_szFunc);
                if (contains && arDbgFilter[i].m_Priority > appliedPriority)
                {
                    if (arDbgFilter[i].m_showFunc == 0)
                        attr = LOG_ATTR_SKIP_FUNC;
                    else if (arDbgFilter[i].m_showFunc == 1)
                        attr = LOG_ATTR_SHOW_FUNC;
                    else if (arDbgFilter[i].m_showFunc == 2)
                        attr = LOG_ATTR_HIDE_FUNC;
                    else if (arDbgFilter[i].m_showFunc == 3)
                        attr = LOG_ATTR_EXPAND_FUNC;
                    appliedPriority = arDbgFilter[i].m_Priority;
                }
            }
        }
    }
    return attr;
}