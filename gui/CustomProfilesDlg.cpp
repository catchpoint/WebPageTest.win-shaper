// CustomProfilesDlg.cpp : implementation file
//

#include "stdafx.h"
#include "gui.h"
#include "CustomProfilesDlg.h"
#include "CustomProfileDlg.h"
#include "afxdialogex.h"


// CustomProfilesDlg dialog

IMPLEMENT_DYNAMIC(CustomProfilesDlg, CDialogEx)

CustomProfilesDlg::CustomProfilesDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_CUSTOM_PROFILES, pParent)
{

}

CustomProfilesDlg::~CustomProfilesDlg()
{
}

void CustomProfilesDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialogEx::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_LIST1, m_profileList);
  DDX_Control(pDX, IDC_EDIT, m_btnEdit);
  DDX_Control(pDX, IDC_DELETE, m_btnDelete);
}


BEGIN_MESSAGE_MAP(CustomProfilesDlg, CDialogEx)
  ON_BN_CLICKED(IDC_ADD, &CustomProfilesDlg::OnBnClickedAdd)
  ON_BN_CLICKED(IDC_EDIT, &CustomProfilesDlg::OnBnClickedEdit)
  ON_BN_CLICKED(IDC_DELETE, &CustomProfilesDlg::OnBnClickedDelete)
  ON_BN_CLICKED(IDOK, &CustomProfilesDlg::OnBnClickedOk)
  ON_LBN_SELCHANGE(IDC_LIST1, &CustomProfilesDlg::OnLbnSelchangeList1)
END_MESSAGE_MAP()


// CustomProfilesDlg message handlers
BOOL CustomProfilesDlg::OnInitDialog()
{
  CDialogEx::OnInitDialog();
  int count = (int)profiles_.profiles_.GetCount();
  for (int i = 0; i < count; i++) {
    m_profileList.InsertString(i, profiles_.profiles_[i].DisplayString());
  }
  if (count > 0) {
    m_profileList.SetCurSel(0);
    m_btnEdit.EnableWindow(TRUE);
    m_btnDelete.EnableWindow(TRUE);
  }

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}


void CustomProfilesDlg::OnBnClickedAdd() {
  CustomProfileDlg dlg;
  dlg.m_inBufferLength = 150000;
  dlg.m_outBufferLen = 150000;
  dlg.m_plr = 0;

  if (dlg.DoModal() == IDOK) {
    ConnectionProfile profile;
    profile.name_ = dlg.m_name;
    profile.inBps_ = dlg.m_inBps;
    profile.outBps_ = dlg.m_outBps;
    profile.rtt_ = dlg.m_rtt;
    profile.plr_ = (int)(dlg.m_plr * 100.0);
    profile.inBufferLen_ = dlg.m_inBufferLength;
    profile.outBufferLen_ = dlg.m_outBufferLen;
    int index = (int)profiles_.profiles_.Add(profile);
    m_profileList.InsertString(index, profile.DisplayString());
  }
}


void CustomProfilesDlg::OnBnClickedEdit() {
  int index = m_profileList.GetCurSel();
  if (index >= 0 && index < profiles_.profiles_.GetCount()) {
    ConnectionProfile &profile = profiles_.profiles_[index];
    CustomProfileDlg dlg;
    dlg.m_name = profile.name_;
    dlg.m_inBps = profile.inBps_;
    dlg.m_outBps = profile.outBps_;
    dlg.m_rtt = profile.rtt_;
    dlg.m_plr = (double)profile.plr_ / 100.0;
    dlg.m_inBufferLength = profile.inBufferLen_;
    dlg.m_outBufferLen = profile.outBufferLen_;
    if (dlg.DoModal() == IDOK) {
      profile.name_ = dlg.m_name;
      profile.inBps_ = dlg.m_inBps;
      profile.outBps_ = dlg.m_outBps;
      profile.rtt_ = dlg.m_rtt;
      profile.plr_ = (int)(dlg.m_plr * 100.0);
      profile.inBufferLen_ = dlg.m_inBufferLength;
      profile.outBufferLen_ = dlg.m_outBufferLen;
      m_profileList.SetRedraw(FALSE);
      m_profileList.DeleteString(index);
      m_profileList.InsertString(index, profile.DisplayString());
      m_profileList.SetRedraw(TRUE);
    }
  }
}


void CustomProfilesDlg::OnBnClickedDelete() {
  int index = m_profileList.GetCurSel();
  int count = (int)profiles_.profiles_.GetCount();
  if (index >= 0 && index < count) {
    if (MessageBox(L"Are you sure you want to delete the connection:\n" + profiles_.profiles_[index].DisplayString(), 0, MB_YESNO) == IDYES) {
      count--;
      profiles_.profiles_.RemoveAt(index);
      m_profileList.DeleteString(index);
      index = max(0, index - 1);
      m_profileList.SetCurSel(index);
    }
  }
  if (profiles_.profiles_.IsEmpty()) {
    m_btnEdit.EnableWindow(FALSE);
    m_btnDelete.EnableWindow(FALSE);
  }
}


void CustomProfilesDlg::OnBnClickedOk() {
  profiles_.Save();
  CDialogEx::OnOK();
}

void CustomProfilesDlg::OnLbnSelchangeList1()
{
  m_btnEdit.EnableWindow(TRUE);
  m_btnDelete.EnableWindow(TRUE);
}
