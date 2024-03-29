#pragma once
#include "Settings.h"
#include "FontPreviewCombo.h"

class DlgSettings : public CDialogImpl<DlgSettings>
{
public:
  enum { IDD = IDD_SETTINGS };

  BEGIN_MSG_MAP(DlgSettings)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
    COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
    COMMAND_HANDLER(IDC_BTN_FONT, BN_CLICKED, OnBnClickedBtnFont)
    COMMAND_HANDLER(IDC_BUTTON_RESET, BN_CLICKED, OnBnClickedButtonReset)
    REFLECT_NOTIFICATIONS()
  END_MSG_MAP()

  // Handler prototypes (uncomment arguments if needed):
  //	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
  //	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
  //	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

  LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
  LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnBnClickedBtnFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
  LRESULT OnBnClickedButtonReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

  void SetFontLabel();
  CStatic m_lblFont;
  CEdit m_QtCreatorPath;
  CButton m_btnFont;
  CButton m_btnReset;
  CButton m_FullSrcPath;
  CButton m_FullFnName;
  CButton m_ShowInQt;

  DWORD     m_FontSize;
  CHAR     m_FaceName[LF_FACESIZE];
  DWORD     m_lfWeight;
};
