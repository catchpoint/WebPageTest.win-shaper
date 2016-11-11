// CustomProfileDlg.cpp : implementation file
//

#include "stdafx.h"
#include "gui.h"
#include "CustomProfileDlg.h"
#include "afxdialogex.h"


// CustomProfileDlg dialog

IMPLEMENT_DYNAMIC(CustomProfileDlg, CDialogEx)

CustomProfileDlg::CustomProfileDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_CUSTOM_PROFILE, pParent)
  , m_name(_T(""))
  , m_inBps(0)
  , m_outBps(0)
  , m_rtt(0)
  , m_plr(0)
  , m_inBufferLength(0)
  , m_outBufferLen(0)
{

}

CustomProfileDlg::~CustomProfileDlg()
{
}

void CustomProfileDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialogEx::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_NAME, m_name);
  DDV_MaxChars(pDX, m_name, 100);
  DDX_Text(pDX, IDC_IN_BPS, m_inBps);
  DDX_Text(pDX, IDC_OUT_BPS, m_outBps);
  DDX_Text(pDX, IDC_RTT, m_rtt);
  DDV_MinMaxLong(pDX, m_rtt, 0, 60000);
  DDX_Text(pDX, IDC_PLR, m_plr);
  DDV_MinMaxDouble(pDX, m_plr, 0, 100);
  DDX_Text(pDX, IDC_IN_BUFF_LEN, m_inBufferLength);
  DDX_Text(pDX, IDC_OUT_BUFF_LEN, m_outBufferLen);
  if (pDX->m_bSaveAndValidate) {
    if (!m_name.Trim().GetLength()) {
      AfxMessageBox(L"Invalid name");
      pDX->PrepareEditCtrl(IDC_NAME);
      pDX->Fail();
    }
  }
}


BEGIN_MESSAGE_MAP(CustomProfileDlg, CDialogEx)
END_MESSAGE_MAP()


// CustomProfileDlg message handlers


void CustomProfileDlg::OnOK()
{
  
  CDialogEx::OnOK();
}
