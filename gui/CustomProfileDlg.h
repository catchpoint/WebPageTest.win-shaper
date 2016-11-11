#pragma once


// CustomProfileDlg dialog

class CustomProfileDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CustomProfileDlg)

public:
	CustomProfileDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CustomProfileDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CUSTOM_PROFILE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
  CString m_name;
  ULONGLONG m_inBps;
  ULONGLONG m_outBps;
  long m_rtt;
  double m_plr;
  ULONGLONG m_inBufferLength;
  ULONGLONG m_outBufferLen;
  virtual void OnOK();
};
