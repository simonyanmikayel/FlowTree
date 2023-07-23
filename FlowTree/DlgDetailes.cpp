#include "stdafx.h"
#include "resource.h"
#include "DlgDetailes.h"
#include "Settings.h"

LRESULT DlgDetailes::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
  CenterWindow(GetParent());

  m_chkNN.Attach(GetDlgItem(IDC_NN));
  m_chkApp.Attach(GetDlgItem(IDC_APP));
  m_chkPID.Attach(GetDlgItem(IDC_PID));
  m_chkThreadNN.Attach(GetDlgItem(IDC_THREAD_NN));
  m_chkTime.Attach(GetDlgItem(IDC_TIME));
  m_chkCallAddr.Attach(GetDlgItem(IDC_CALL_ADDR));
  m_chkFnCallLine.Attach(GetDlgItem(IDC_FN_CALL_LINE));
  m_chkFuncName.Attach(GetDlgItem(IDC_FUNC_NAME));
  m_chkCallLine.Attach(GetDlgItem(IDC_CALL_LINE));
  m_ShowElapsedTime.Attach(GetDlgItem(IDC_CHECK_SHOW_ELAPSED_TIME));

  m_chkNN.SetCheck(gSettings.ColNN());
  m_chkApp.SetCheck(gSettings.ColApp());
  m_chkPID.SetCheck(gSettings.ColPID());
  m_chkThreadNN.SetCheck(gSettings.ColThreadNN());
  m_chkTime.SetCheck(gSettings.ColTime());
  m_chkCallAddr.SetCheck(gSettings.ColCallAddr());
  m_chkFnCallLine.SetCheck(gSettings.FnCallLine());
  m_chkFuncName.SetCheck(gSettings.ColFunc());
  m_chkCallLine.SetCheck(gSettings.ColLine());
  m_ShowElapsedTime.SetCheck(gSettings.ShowElapsedTime());

  return TRUE;
}

LRESULT DlgDetailes::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
  if (wID == IDOK)
  {
    gSettings.ColNN(m_chkNN.GetCheck());
	gSettings.ColApp(m_chkApp.GetCheck());
	gSettings.ColPID(m_chkPID.GetCheck());
	gSettings.ColThreadNN(m_chkThreadNN.GetCheck());
    gSettings.ColTime(m_chkTime.GetCheck());
    gSettings.ColCallAddr(m_chkCallAddr.GetCheck());
    gSettings.FnCallLine(m_chkFnCallLine.GetCheck());
    gSettings.ColFunc(m_chkFuncName.GetCheck());
    gSettings.ColLine(m_chkCallLine.GetCheck());
    gSettings.ShowElapsedTime(m_ShowElapsedTime.GetCheck());
  }
  EndDialog(wID);
  return 0;
}
