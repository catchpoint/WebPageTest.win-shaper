
// WinShaperDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "CustomProfiles.h"
#include "afxcmn.h"

// CWinShaperDlg dialog
class CWinShaperDlg : public CDialogEx
{
// Construction
public:
	CWinShaperDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_GUI_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnCbnSelchangeConnectionProfiles();
  afx_msg void OnBnClickedEnable();
protected:
  void PopulateConnectionList();
  void LoadProfiles();
  void Enable();
  void Disable();
  bool Install();
  void Uninstall();
  bool Start();
  void Stop();
  CString ExtractDriver();
  void Error(CString message);
  void UpdateStatus();

  bool enabled_;
  CComboBox m_profileList;
  CButton m_btnEnable;
  CArray<ConnectionProfile> connection_profiles_;
  virtual void OnOK();
  virtual void OnCancel();
  CString driver_path_;
  HANDLE  driver_interface_;
  CustomProfiles custom_;
public:
  afx_msg void OnClose();
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  CProgressCtrl m_inboundQueue;
  CProgressCtrl m_outboundQueue;
};
