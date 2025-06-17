#pragma once

struct LOG_NODE;
struct FLOW_NODE;
extern HWND hwndMain;
extern FLOW_NODE*  gSyncronizedNode;
#define WM_UPDATE_FILTER               WM_USER + 1000
#define WM_INPORT_TASK                 WM_USER + 1001
#define WM_MOVE_SELECTION_TO_END       WM_USER + 1002
#define WM_SHOW_NGS                    WM_USER + 1003
#define WM_UPDATE_BACK_TRACE           WM_USER + 1004

static const int ICON_LEN = 16;
static const int ICON_OFFSET = 16 + 4;
static const int TEXT_MARGIN = 8;
static const int INFO_TEXT_MARGIN = 4;
enum MENU_ICON { MENU_ICON_NON = -1, MENU_ICON_SYNC, MENU_ICON_FUNC_IN_STUDIO, MENU_ICON_CALL_IN_STUDIO, MENU_ICON_MAX};
namespace Helpers
{
  void CopyToClipboard(HWND hWnd, char* szText, int cbText);
  std::string CommErr(HRESULT hr);
  void SysErrMessageBox(CHAR* lpFormat, ...);
  void ErrMessageBox(CHAR* lpFormat, ...);
  CHAR* find_str(const CHAR *phaystack, const CHAR *pneedle, int bMatchCase);  
  CHAR* str_format_int_grouped(__int64 num);
  void GetTime(DWORD &sec, DWORD& msec);
  void CloseSocket(SOCKET &s);
  void OnLButtonDoun(LOG_NODE* pNode, WPARAM wParam, LPARAM lParam);
  void ShowInIDE(LOG_NODE* pNode, bool bShowCallSite);
  void AddCommonMenu(LOG_NODE* pNode, HMENU hMenu, int& cMenu);
  void AddMenu(HMENU hMenu, int& cMenu, int ID_MENU, LPCTCH str, bool disable = false, MENU_ICON ID_ICON = MENU_ICON_NON);
  void SetMenuIcon(HMENU hMenu, UINT item, MENU_ICON icon);
  void UpdateStatusBar();
  void UpdateStatusBar(char* str);
  // convert UTF-8 string to wstring
  std::wstring utf8_to_wstring(const std::string& str);
  // convert wstring to UTF-8 string
  std::string wstring_to_utf8(const std::wstring& str);
  // convert wchar_t to UTF-8 string
  std::string wchar_to_utf8(const wchar_t* sz);
  // convert char to wstring
  std::wstring char_to_wstring(const char* sz);
  void ClearLog();
  uint32_t crc32(const std::string& data);
  void UpdateDbgLoadStatus();
  void BlockLogStatus(bool b);
  bool BlockLogStatus();
};


//case insensitive search
template <typename CharType> inline bool FindStrIC(const CharType* p, const CharType* str)
{
    CharType* pch = (CharType*)str;
    const CharType* begin = NULL;
    while (*p)
    {
        if (toupper(*pch) == toupper(*p))
        {
            if (!begin)
                begin = p;
            pch++;
        }
        else
        {
            if (begin)
            {
                p = begin;
                begin = NULL;
            }
            pch = (CharType*)str;
        }
        if (*pch == 0)
            break;
        p++;
    }
    return (*pch == 0);
}