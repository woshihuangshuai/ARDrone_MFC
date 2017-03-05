
// ADDRONE_MFCDlg.h : 头文件
//

#pragma once
#include "ARDrone.h"
#include "afxcmn.h"

// CADDRONE_MFCDlg 对话框
class CADDRONE_MFCDlg : public CDialogEx
{
// 构造
public:
	CADDRONE_MFCDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_ADDRONE_MFC_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
	//额外添加的变量

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	CProgressCtrl battery_level_progress;
	afx_msg void OnBnClickedstartconnection();
	afx_msg void OnBnClickedcloseconnection();
	afx_msg void AppendText(int controlId, CString strAdd);
	afx_msg void ClearText(int controlId);
	afx_msg void OnBnClickednavmodestart();
	afx_msg void OnBnClickednavmodeclose();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedkeyboardmodestart();
	afx_msg void OnBnClickedkeyboardmodeclose();
	afx_msg void OnBnClickedfrontcamera();
	afx_msg void OnBnClickeddowncamera();
	afx_msg void OnBnClickedButton1();
};
