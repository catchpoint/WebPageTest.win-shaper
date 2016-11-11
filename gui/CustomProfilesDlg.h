#pragma once
#include "CustomProfiles.h"
#include "afxwin.h"

// CustomProfilesDlg dialog

class CustomProfilesDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CustomProfilesDlg)

public:
	CustomProfilesDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CustomProfilesDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CUSTOM_PROFILES };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnBnClickedAdd();
  afx_msg void OnBnClickedEdit();
  afx_msg void OnBnClickedDelete();
  afx_msg void OnBnClickedOk();
  virtual BOOL OnInitDialog();

  CustomProfiles profiles_;
  CListBox m_profileList;
  CButton m_btnEdit;
  CButton m_btnDelete;
  afx_msg void OnLbnSelchangeList1();
};
