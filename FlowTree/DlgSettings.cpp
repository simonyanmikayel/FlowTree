#include "stdafx.h"
#include "resource.h"
#include "DlgSettings.h"
#include "Settings.h"

#define AS(s) m_cmbFontSize.AddString(#s)

LRESULT DlgSettings::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  m_lblFont.Attach(GetDlgItem(IDC_FONT_NAME));
  m_QtCreatorPath.Attach(GetDlgItem(IDC_EDIT_QT_CREATOR_PATH));
  m_CLionPath.Attach(GetDlgItem(IDC_EDIT_CLION_PATH));
  m_btnFont.Attach(GetDlgItem(IDC_BTN_FONT));
  m_btnReset.Attach(GetDlgItem(IDC_BUTTON_RESET));
  m_FullSrcPath.Attach(GetDlgItem(IDC_CHECK_FULL_SRC_PATH));
  m_FullFnName.Attach(GetDlgItem(IDC_CHECK_FULL_FN_NAME));
  m_DissableBufferization.Attach(GetDlgItem(IDC_CHECK_FULL_DISSABLE_BUFFERIZATION));
  m_UseTcpForLog.Attach(GetDlgItem(IDC_CHECK_USETCPFORLOG));
  //m_IdeType.Attach(GetDlgItem(IDC_RADIO_VS));
  
  m_QtCreatorPath.SetWindowText(gSettings.QtCreatorPath());
  m_CLionPath.SetWindowText(gSettings.CLionPath());
  m_FontSize = gSettings.FontSize();
  strncpy_s(m_FaceName, _countof(m_FaceName), gSettings.FontName(), _countof(m_FaceName) - 1);
  m_FaceName[LF_FACESIZE - 1] = 0;
  m_lfWeight = gSettings.FontWeight();
  SetFontLabel();

  m_FullSrcPath.SetCheck(gSettings.FullSrcPath() ? BST_CHECKED : BST_UNCHECKED);
  m_FullFnName.SetCheck(gSettings.FullFnName() ? BST_CHECKED : BST_UNCHECKED);
  m_DissableBufferization.SetCheck(gSettings.DissableBufferization() ? BST_CHECKED : BST_UNCHECKED);
  m_UseTcpForLog.SetCheck(gSettings.UseTcpForLog() ? BST_CHECKED : BST_UNCHECKED);
  CheckRadioButton(IDC_RADIO_VS, IDC_RADIO_CLION, IDC_RADIO_VS + gSettings.IdeType());
  //m_IdeType.SetCheck(gSettings.IdeType() ? BST_CHECKED : BST_UNCHECKED);
  CenterWindow(GetParent());

#ifdef USE_PIPE
  GetDlgItem(IDC_CHECK_FULL_DISSABLE_BUFFERIZATION).EnableWindow(FALSE);
  GetDlgItem(IDC_CHECK_USETCPFORLOG).EnableWindow(FALSE);
#endif //USE_PIPE

  return TRUE;
}

void DlgSettings::SetFontLabel()
{
  CString str;
  str.Format(_T("Font: %s, %s%d point "), m_FaceName, m_lfWeight >= FW_SEMIBOLD  ? _T("Bold ") : _T(""), m_FontSize);
  m_lblFont.SetWindowText(str);
}

LRESULT DlgSettings::OnBnClickedBtnFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  CHOOSEFONT cf;
  HDC hdc;
  LOGFONT lf;

  hdc = ::GetDC(0);
  lf.lfHeight = -MulDiv(m_FontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
  ::ReleaseDC(0, hdc);

  _tcsncpy_s(lf.lfFaceName, _countof(lf.lfFaceName), m_FaceName, _countof(lf.lfFaceName) - 1);
  lf.lfFaceName[_countof(lf.lfFaceName) - 1] = 0;
  lf.lfWidth = lf.lfEscapement = lf.lfOrientation = 0;
  lf.lfItalic = lf.lfUnderline = lf.lfStrikeOut = 0;
  lf.lfWeight = (m_lfWeight >= FW_SEMIBOLD ? FW_BOLD : FW_NORMAL);
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
  lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  lf.lfQuality = DEFAULT_QUALITY;
  lf.lfPitchAndFamily = FF_DONTCARE; // | FIXED_PITCH

  ZeroMemory(&cf, sizeof(cf));
  cf.lStructSize = sizeof(cf);
  cf.hwndOwner = m_hWnd;
  cf.lpLogFont = &lf;
  cf.Flags = CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_INACTIVEFONTS;// | CF_FIXEDPITCHONLY;

  if (ChooseFont(&cf)) {
    m_FontSize = cf.iPointSize / 10;
    _tcsncpy_s(m_FaceName, _countof(m_FaceName), lf.lfFaceName, _countof(m_FaceName) - 1);
    m_FaceName[LF_FACESIZE - 1] = 0;
    m_lfWeight = (lf.lfWeight >= FW_SEMIBOLD ? FW_BOLD : FW_NORMAL);
    SetFontLabel();
  }

  return 0;
}

LRESULT DlgSettings::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (wID == IDOK)
  {
    gSettings.SetUIFont(m_FaceName, m_lfWeight, m_FontSize);
    gSettings.FullSrcPath(m_FullSrcPath.GetCheck());
	gSettings.FullFnName(m_FullFnName.GetCheck());
    gSettings.DissableBufferization(m_DissableBufferization.GetCheck());
    gSettings.UseTcpForLog(m_UseTcpForLog.GetCheck());
    //int ideType = m_IdeType.GetCheck();
    int ideType = 0;
    if (IsDlgButtonChecked(IDC_RADIO_VS))
        ideType = 0;
    else if (IsDlgButtonChecked(IDC_RADIO_QT))
        ideType = 1;
    else if (IsDlgButtonChecked(IDC_RADIO_CLION))
        ideType = 2;
    gSettings.IdeType(ideType);

    CString strQtCreatorPath;
    m_QtCreatorPath.GetWindowText(strQtCreatorPath);
    gSettings.SetQtCreatorPath(strQtCreatorPath.GetString());

    CString strCLionPath;
    m_CLionPath.GetWindowText(strCLionPath);
    gSettings.SetCLionPath(strCLionPath.GetString());
    

  }
  EndDialog(wID);
  return 0;
}

LRESULT DlgSettings::OnBnClickedButtonReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  return 0;
}
