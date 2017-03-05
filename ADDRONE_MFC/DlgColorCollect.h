#pragma once


// CDlgColorCollect �Ի���

class CDlgColorCollect : public CDialogEx
{
	DECLARE_DYNAMIC(CDlgColorCollect)

public:
	CDlgColorCollect(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CDlgColorCollect();

	// �Ի�������
	enum { IDD = IDD_COLOR_CELLECT };

	IplImage* img;
	IplImage* hsv_img;

	CvScalar hsv;
	CvScalar hsv_min;
	CvScalar hsv_max;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual INT_PTR DoModal();
	afx_msg void OnPaint();
	afx_msg void OnBnClickedOk();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
};
