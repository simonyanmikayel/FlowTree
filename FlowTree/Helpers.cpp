#include "stdafx.h"
#include "Helpers.h"
#include "Resource.h"
#include "comdef.h"
#include "MainFrm.h"
#include "Settings.h"

namespace Helpers
{
    void OnLButtonDoun(LOG_NODE* pNode, WPARAM wParam, LPARAM lParam)
    {
        bool bAltPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool bCtrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool bMbuttonPressed = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
        if (bCtrlPressed || bMbuttonPressed)
        {
            ShowInIDE(pNode, true);
        }
        else if (bAltPressed)
        {
            ShowInIDE(pNode, false);
        }
    }


    char* DteAppName()
    {
        static char szAppFullName[MAX_PATH + 16] = { 0 };
        if (szAppFullName[0] == 0)
        {
            GetModuleFileNameA(NULL, szAppFullName, _countof(szAppFullName));
            char *lastSlash = strrchr(szAppFullName, '\\');
            if (!lastSlash)
                lastSlash = strrchr(szAppFullName, '/');
            if (lastSlash)
                *lastSlash = 0;
            strcat_s(szAppFullName, "\\Dte.exe");
        }
        return szAppFullName;
    }

    void ShowInIDE(LOG_NODE* pSelectedNode, bool bShowCallSite)
    {
        if (bShowCallSite) {
#ifndef HAVE_CALL_LINE
            return;
#endif
        }
        
        if (pSelectedNode)
        {
            STARTUPINFO si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            //D:\Programs\eclipse\eclipse-cpp-neon-M4a-win32-x86_64\eclipsec.exe -name Eclipse --launcher.openFile X:\prj\c\c\ctap\kernel\CTAPparameters\src\CTAP_parameters.c:50
            const int max_cmd = 2 * MAX_PATH;
            char cmd[max_cmd + 1];
            int line = 0;
            char* src = "";
            if (pSelectedNode->isTrace())
            {
                TRACE_NODE* pTraceNode = (TRACE_NODE*)pSelectedNode;
                line = pTraceNode->GetCallLine();
                LOG_NODE* pNode = pTraceNode->getSyncNode();
                if (pNode->isFlow())
                {
                    FLOW_NODE* pFlowNode = (FLOW_NODE*)pNode;
                    src = pFlowNode->getFuncSrc(true);
                }
            }
            else if (pSelectedNode->isFlow())
            {
                FLOW_NODE* pFlowNode = (FLOW_NODE*)pSelectedNode;
                if (bShowCallSite)
                {
                    src = pFlowNode->getCallSrc(true);
                    line = pFlowNode->GetCallLine();
                }
                else
                {
                    src = pFlowNode->getFuncSrc(true);
                    line = pFlowNode->GetFuncLine();
                }
            }
            if (src[0] == 0 || line < 0 || line > 1000000)
                return;
            if (line <= 0)
                line = 1;

            if (gSettings.ShowInQt()) {
                int cbCmd = _sntprintf_s(cmd, max_cmd, max_cmd, "\"%s\" -client \"%s:%d\"", gSettings.QtCreatorPath(), src, line);
                PROCESS_INFORMATION pi;
                if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                    NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
                {
                    Helpers::SysErrMessageBox("Failed to run Qt creator at path %s", gSettings.QtCreatorPath());
                }
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            else {
                int cbCmd = _sntprintf_s(cmd, max_cmd, max_cmd, " %s %d", src, line);
                char* srcPos = StrRStrIA(cmd, cmd + cbCmd, src);
                char* srcEnd = srcPos + strlen(src);
                while (srcPos < srcEnd)
                {
                    if (*srcPos == ' ')
                        *srcPos = '*';
                    srcPos++;
                }
                PROCESS_INFORMATION pi;
                if (!CreateProcessA(DteAppName(), cmd, NULL, NULL, FALSE,
                    NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
                {
                    Helpers::SysErrMessageBox("Failed to run %s", DteAppName());
                }
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
    }

    void CloseSocket(SOCKET &s)
    {
        SOCKET s0 = s;
        s = INVALID_SOCKET;
        if (s0 != INVALID_SOCKET)
            closesocket(s0);
    }
    void CopyToClipboard(HWND hWnd, char* szText, int cbText)
    {
        if (!szText || !cbText)
            return;
		if (cbText == -1)
			cbText = (int)strlen(szText);
        char *lock = 0;
        HGLOBAL clipdata = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, (cbText + 1) * sizeof(char));
        if (!clipdata)
            return;
        if (!(lock = (char*)GlobalLock(clipdata))) {
            GlobalFree(clipdata);
            return;
        }

        memcpy(lock, szText, cbText);
        memcpy(lock + cbText, "", 1);

        GlobalUnlock(clipdata);
        if (::OpenClipboard(hWnd)) {
            EmptyClipboard();
            SetClipboardData(CF_TEXT, clipdata);
            CloseClipboard();
        }
        else {
            GlobalFree(clipdata);
        }
    }

    void ErrMessageBox(CHAR* lpFormat, ...)
    {
        va_list vl;
        va_start(vl, lpFormat);

        CHAR* buf = new CHAR[1024];
        _vsntprintf_s(buf, 1023, 1023, lpFormat, vl);
        va_end(vl);

        ::PostMessage(hwndMain, WM_SHOW_NGS, (WPARAM)buf, (LPARAM)0);
    }

	std::string CommErr(HRESULT hr)
	{
		std::string str;
		_com_error err(hr);
		LPCTSTR errMsg = err.ErrorMessage();
#ifdef  UNICODE
		std::wstring wstr = errMsg;
		str wstring_to_utf8(wstr);
		typedef std::wstring tstring;
#else
		str = errMsg;
#endif
		return str;
	}

    void SysErrMessageBox(CHAR* lpFormat, ...)
    {
        DWORD err = GetLastError();
        CHAR *s = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            s, 0, NULL);

        va_list vl;
        va_start(vl, lpFormat);

        CHAR buf[1024];
        _vsntprintf_s(buf, _countof(buf), lpFormat, vl);

        ErrMessageBox(TEXT("%s\nErr: %d\n%s"), buf, err, s);
        va_end(vl);

        LocalFree(s);
    }

    typedef unsigned chartype;

    wchar_t char_table[256];
    static int initialised = 0;

    void init_stristr(void)
    {
        for (int i = 0; i < 256; i++)
        {
            char_table[i] = towupper(i);
        }
    }

#define uppercase(_x) ( (chartype) (bMatchCase ? _x : (_x < 256 ? char_table[_x] : towupper(_x) )) )

    CHAR* find_str(const CHAR *phaystack, const CHAR *pneedle, int bMatchCase)
    {
        register const CHAR *haystack, *needle;
        register chartype b, c;

        if (!initialised)
        {
            initialised = 1;
            init_stristr();
        }

        if (!phaystack || !pneedle || !pneedle[0])
            goto ret0;

        haystack = (const CHAR *)phaystack;
        needle = (const CHAR *)pneedle;
        b = uppercase(*needle);

        haystack--;             /* possible ANSI violation */
        do
        {
            c = *++haystack;
            if (c == 0)
                goto ret0;
        } while (uppercase(c) != (int)b);

        c = *++needle; //dont pass ++needle to macros
        c = uppercase(c);
        if (c == '\0')
            goto foundneedle;

        ++needle;
        goto jin;

        for (;;)
        {
            register chartype a;
            register const CHAR *rhaystack, *rneedle;

            do
            {
                a = *++haystack;
                if (a == 0)
                    goto ret0;
                if (uppercase(a) == (int)b)
                    break;
                a = *++haystack;
                if (a == 0)
                    goto ret0;
            shloop:
                ;
            } while (uppercase(a) != (int)b);

        jin:
            a = *++haystack;
            if (a == 0)
                goto ret0;

            if (uppercase(a) != (int)c)
                goto shloop;

            rhaystack = haystack-- + 1;
            rneedle = needle;

            a = uppercase(*rneedle);
            if (uppercase(*rhaystack) == (int)a)
                do
                {
                    if (a == 0)
                        goto foundneedle;

                    ++rhaystack;
                    a = *++needle;
                    a = uppercase(a);
                    if (uppercase(*rhaystack) != (int)a)
                        break;

                    if (a == '\0')
                        goto foundneedle;

                    ++rhaystack;
                    a = *++needle;
                    a = uppercase(a);
                } while (uppercase(*rhaystack) == (int)a);

                needle = rneedle;       /* took the register-poor approach */

                if (a == 0)
                    break;
        }

    foundneedle:
        return (CHAR*)haystack;
    ret0:
        return 0;
    }

    CHAR* str_format_int_grouped(__int64 num)
    {
        static CHAR dst[16];
        CHAR src[16];
        char *p_src = src;
        char *p_dst = dst;

        const char separator = ',';
        int num_len, commas;

        num_len = sprintf_s(src, _countof(src), "%lld", num);

        if (*p_src == '-') {
            *p_dst++ = *p_src++;
            num_len--;
        }

        for (commas = 2 - num_len % 3; *p_src; commas = (commas + 1) % 3) {
            *p_dst++ = *p_src++;
            if (commas == 1) {
                *p_dst++ = separator;
            }
        }
        *--p_dst = '\0';
        return dst;
    }
    void GetTime(DWORD &sec, DWORD& msec)
    {
        //__int64 wintime; 
        //GetSystemTimeAsFileTime((FILETIME*)&wintime);
        //wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
        //sec = wintime / 10000000i64;           //seconds
        //nano_sec = wintime % 10000000i64 * 100;      //nano-second

        SYSTEMTIME st;
        GetLocalTime(&st);

        //sec = _time32(NULL);
        msec = st.wMilliseconds;

        tm local;
        memset(&local, 0, sizeof(local));

        local.tm_year = st.wYear - 1900;
        local.tm_mon = st.wMonth - 1;
        local.tm_mday = st.wDay;
        local.tm_wday = st.wDayOfWeek;
        local.tm_hour = st.wHour;
        local.tm_min = st.wMinute;
        local.tm_sec = st.wSecond;

        sec = (DWORD)(mktime(&local));
    }

    void AddMenu(HMENU hMenu, int& cMenu, int ID_MENU, LPCTCH str, bool disable, MENU_ICON ID_ICON)
    {
        DWORD dwFlags;
        dwFlags = MF_BYPOSITION | MF_STRING;
        if (disable)
            dwFlags |= MF_DISABLED;
        InsertMenu(hMenu, cMenu++, dwFlags, ID_MENU, str);
        if (ID_ICON >= 0)
            Helpers::SetMenuIcon(hMenu, cMenu - 1, ID_ICON);
    }

    void AddCommonMenu(LOG_NODE* pNode, HMENU hMenu, int& cMenu)
    {
        bool disable;
        disable = (pNode == NULL || !pNode->isInfo());
        AddMenu(hMenu, cMenu, ID_SYNC_VIEWES, _T("Synchronize views\tTab"), disable, MENU_ICON_SYNC);
#ifdef HAVE_CALL_LINE
        disable = (pNode == NULL || !pNode->isInfo());
        AddMenu(hMenu, cMenu, ID_SHOW_CALL_IN_STUDIO, _T("Show Call Line\tCtrl+Click"), disable);
#endif

#ifdef HAVE_CALL_LINE
        disable = (pNode == NULL || !pNode->isInfo() || pNode->isTrace());
#else
        disable = (pNode == NULL || !pNode->isInfo());
#endif
        AddMenu(hMenu, cMenu, ID_SHOW_FUNC_IN_STUDIO, _T("Show Function\tAlt+Click"), disable);

        InsertMenu(hMenu, cMenu++, MF_BYPOSITION | MF_SEPARATOR, 0, _T(""));
    }

    void SetMenuIcon(HMENU hMenu, UINT item, MENU_ICON icon)
    {
        static HBITMAP hbmpItem[MENU_ICON_MAX] = { 0 };

        if (icon >= MENU_ICON_MAX)
            return;
        if (hbmpItem[icon] == 0)
        {
            int iRes = -1;
            if (icon == MENU_ICON_SYNC)
                iRes = IDB_SYNC;
            else if (icon == MENU_ICON_FUNC_IN_STUDIO)
                iRes = IDB_FUNC_IN_STUDIO;
            else if (icon == MENU_ICON_CALL_IN_STUDIO)
                iRes = IDB_CALL_IN_STUDIO;
            if (iRes >= 0)
            {
                hbmpItem[icon] = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(iRes));
            }
        }
        MENUITEMINFO mii;
        mii.cbSize = sizeof(MENUITEMINFO);
        mii.fMask = MIIM_BITMAP;
        GetMenuItemInfo(hMenu, item, TRUE, &mii);
        mii.hbmpItem = hbmpItem[icon];
        SetMenuItemInfo(hMenu, item, TRUE, &mii);
    }


    // convert UTF-8 string to wstring
    std::wstring utf8_to_wstring(const std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        return convert.from_bytes(str);
    }
    // convert wstring to UTF-8 string
    std::string wstring_to_utf8(const std::wstring& str)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        return convert.to_bytes(str);
    }
    // convert wchar to UTF-8 string
    std::string wchar_to_utf8(const wchar_t* sz)
    {
        std::wstring str(sz);
        return wstring_to_utf8(str);
    }
    // convert char to wstring
    std::wstring char_to_wstring(const char* sz)
    {
        std::string str(sz);
        return utf8_to_wstring(str);
    }

    void UpdateStatusBar()
    {
        gMainFrame->UpdateStatusBar();
    }
};

