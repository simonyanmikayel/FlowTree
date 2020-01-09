#include "stdafx.h"
#include "resource.h"
#include "DlgSettings.h"
#include "Settings.h"

#define AS(s) m_cmbFontSize.AddString(#s)

LRESULT DlgSettings::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  m_lblFont.Attach(GetDlgItem(IDC_FONT_NAME));
  m_UdpPort.Attach(GetDlgItem(IDC_EDIT_PORT));
  m_btnFont.Attach(GetDlgItem(IDC_BTN_FONT));
  m_btnReset.Attach(GetDlgItem(IDC_BUTTON_RESET));
  m_FullSrcPath.Attach(GetDlgItem(IDC_CHECK_FULL_SRC_PATH));
  m_FullFnName.Attach(GetDlgItem(IDC_CHECK_FULL_FN_NAME));
  
  m_FontSize = gSettings.GetFontSize();
  strncpy_s(m_FaceName, _countof(m_FaceName), gSettings.GetFontName(), _countof(m_FaceName) - 1);
  m_FaceName[LF_FACESIZE - 1] = 0;
  m_lfWeight = gSettings.GetFontWeight();
  SetFontLabel();

  SetDlgItemInt(IDC_EDIT_PORT, gSettings.GetUdpPort(), FALSE);
  m_FullSrcPath.SetCheck(gSettings.GetFullSrcPath() ? BST_CHECKED : BST_UNCHECKED);
  m_FullFnName.SetCheck(gSettings.GetFullFnName() ? BST_CHECKED : BST_UNCHECKED);
  CenterWindow(GetParent());
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
    gSettings.SetUdpPort(GetDlgItemInt(IDC_EDIT_PORT));
    gSettings.SetFullSrcPath(m_FullSrcPath.GetCheck());
	gSettings.SetFullFnName(m_FullFnName.GetCheck());
  }
  EndDialog(wID);
  return 0;
}

LRESULT DlgSettings::OnBnClickedButtonReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  return 0;
}
