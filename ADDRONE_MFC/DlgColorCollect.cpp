// DlgColorCollect.cpp : 实现文件
//

#include "stdafx.h"
#include "ADDRONE_MFC.h"
#include "DlgColorCollect.h"
#include "afxdialogex.h"

CRect color_rect;  
CDC *color_pDC;  
HDC color_hDC;  
CWnd *color_pwnd; 
// CDlgColorCollect 对话框

IMPLEMENT_DYNAMIC(CDlgColorCollect, CDialogEx)

	CDlgColorCollect::CDlgColorCollect(CWnd* pParent /*=NULL*/)
	: CDialogEx(CDlgColorCollect::IDD, pParent)
{

}

CDlgColorCollect::~CDlgColorCollect()
{
}

void CDlgColorCollect::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgColorCollect, CDialogEx)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDOK, &CDlgColorCollect::OnBnClickedOk)
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()


// CDlgColorCollect 消息处理程序


BOOL CDlgColorCollect::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
}


INT_PTR CDlgColorCollect::DoModal()
{
	// TODO: 在此添加专用代码和/或调用基类
	return CDialogEx::DoModal();
}


void CDlgColorCollect::OnPaint()
{
	cv::Mat a = cv::imread("sample.jpg");
	img = new IplImage(a);

	color_pwnd = GetDlgItem(IDC_img);   
	color_pDC =color_pwnd->GetDC();   
	color_hDC = color_pDC->GetSafeHdc();  
	color_pwnd->GetClientRect(&color_rect); 
	CvvImage m_CvvImage;
	m_CvvImage.CopyOf(this->img,1);       
	m_CvvImage.DrawToHDC(color_hDC, &color_rect);

	hsv_img = cvCreateImage(cvGetSize(img),IPL_DEPTH_8U,3);
	cvCvtColor(img,hsv_img,CV_BGR2HSV);

	CPaintDC dc(this); // device context for painting
}


void CDlgColorCollect::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码
	CDialogEx::OnOK();
}


BOOL CDlgColorCollect::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类
	if(pMsg->message == WM_LBUTTONDOWN && GetDlgItem(IDC_img)->GetSafeHwnd() == pMsg->hwnd)
		OnLButtonDown(MK_LBUTTON, pMsg->pt);   //在此传递点击部位在对话框中的坐标
	return CDialogEx::PreTranslateMessage(pMsg);
}


void CDlgColorCollect::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if((point.x>=color_rect.left && point.x<=color_rect.right) && (point.y>=color_rect.top && point.y<=color_rect.bottom))
	{
		//  通过对point的处理，获得实际在picture控件中的点击位置（坐标），完成。
		point.x-=color_rect.left;
		point.y-=color_rect.top;
	}
	int width = color_rect.Width();
	int height = color_rect.Height();
	int x = point.x / width * 640;
	int y = point.y / height * 480;

	hsv = cvGet2D(hsv_img,point.y,point.x);
	char text[20];
	sprintf_s(text,"%d,%d,%d",(int)(hsv.val[0]*2),(int)(hsv.val[1]/2.55),(int)(hsv.val[2]/2.55));
	AfxMessageBox(text);
	CDialogEx::OnLButtonDown(nFlags, point);
}
