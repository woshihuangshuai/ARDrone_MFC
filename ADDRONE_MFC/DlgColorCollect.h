#pragma once


// CDlgColorCollect 对话框

class CDlgColorCollect : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgColorCollect)

public:
	CDlgColorCollect(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgColorCollect();

	// 对话框数据
	enum { IDD = IDD_COLOR_CELLECT };

	IplImage* img;
	IplImage* hsv_img;

	CvScalar hsv;
	CvScalar hsv_min;
	CvScalar hsv_max;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual INT_PTR DoModal();
	afx_msg void OnPaint();
	afx_msg void OnBnClickedOk();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
};
