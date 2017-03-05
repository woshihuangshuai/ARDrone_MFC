
// ADDRONE_MFCDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "ADDRONE_MFC.h"
#include "ADDRONE_MFCDlg.h"
#include "afxdialogex.h"
#include "ARDrone.h"
#include "DlgColorCollect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

int goal_yaw = 100;

const int FLYING = 1;
const int LANDING = 0;
int drone_state = LANDING;

//控制模式
int control_mode;
const int NAV_MODE = 1;
const int KEYBOARD_MODE = 2;
const int CONNECT_MODE = 0;
const int DISCONNECT_MODE = -1;
const int LEFT = +1;
const int RIGHT = -1;
const int UP = +1;
const int DOWN = -1;
const int GET_GOAL = 0;
const int NOT_FIND = -2;
int v = NOT_FIND;	//0,+1,-1,垂直方向
int h = NOT_FIND;	//0,+1,-1，水平方向

//图像显示
CRect rect;  
CRect rect2;
CDC *pDC ,*pDC2;  
HDC hDC,hDC2;  
CWnd *pwnd,*pwnd2; 

//图像处理相关变量
//CvScalar hsv_min = {105,200,50,0};
//CvScalar hsv_max = {120,255,250,0};

////蓝色
//CvScalar hsv_min = {105,150,100,0};
//CvScalar hsv_max = {120,255,200,0};

//红色
CvScalar hsv_min = {120,100,100,0};
CvScalar hsv_max = {190,255,255,0};

ARDrone *ardrone;
cv::VideoCapture *capture;

/*********************工作线程***********************/
CWinThread* wakeup_thread = NULL;
CWinThread* navdata_thread = NULL;
CWinThread* control_thread = NULL;

// 保持与Ardrone连接的线程
UINT WINAPI WakeUpThread(LPVOID pParam)
{
	char cmd[1024] = { 0 };
	int delay = 0;
	while (true)
	{
		if(control_mode == NAV_MODE) 
		{
			Sleep(40);
			continue;
		}
		// keep weak up
		Sleep(10);
		if (ardrone->getCurrentSeq() == ardrone->getLastSeq())
			ardrone->send_at_cmd(ardrone->at_cmd_last);

		ardrone->setLastSeq(ardrone->getCurrentSeq());
		delay++;
		if (delay >= 4)
		{
			delay = 0;
			sprintf_s(cmd, "AT*COMWDG=%d\r", ardrone->getNextSeq());
			assert(ardrone->send_at_cmd(cmd));
		}
	}
}

// 获取导航数据的线程
UINT WINAPI NavDataThread(LPVOID pParam)
{
	SOCKET socketNav_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in	navSin_;
	navSin_.sin_family = AF_INET;
	navSin_.sin_port = htons(NAVDATA_PORT);
	navSin_.sin_addr.s_addr = inet_addr(ARDrone_IP);
	int lenNavSin_ = sizeof(navSin_);

	// 激活指令发往NAVDATA_PORT 端口
	const char trigger[4] = { 0x01, 0x00, 0x00, 0x00 };
	int result = sendto(socketNav_, trigger, strlen(trigger), 0, (sockaddr *)&navSin_, lenNavSin_);
	if (result != SOCKET_ERROR)
	{
		printf_s("Sent trigger flag to UDP port : %d \n", NAVDATA_PORT);
	}

	// 配置指令发往AT_PORT 端口
	char initCmd[1024] = { 0 };
	sprintf_s(initCmd, "AT*CONFIG=%d,\"general:navdata_demo\",\"TRUE\"\r", ardrone->getNextSeq());
	assert(ardrone->send_at_cmd(initCmd));

	// 接收数据包
	MemoryLibrary::Buffer navDataBuffer;	// 二进制数据缓冲区
	char recv[1024] = { 0 };				// 数据包接收数组
	int lenRecv = 0;
	int delay = 0;
	// UI 部分
	char text[100] = { 0 };					// 导航数据显示字符串

	while (true)
	{
		lenRecv = recvfrom(socketNav_, recv, 1024, 0, (struct sockaddr*)&navSin_, &lenNavSin_);
		delay++;
		if (delay >= 5)
		{
			delay = 0;
			printf("received %d bytes\n", lenRecv);
			navDataBuffer.Set(recv, lenRecv);
			ardrone->parse(navDataBuffer);

			sprintf_s(text, "batreeyLevel: %d%%  altitude: %d\n"
				, ardrone->navData.batteryLevel, ardrone->navData.altitude);
		}
	}
}

// 视频处理线程
UINT WINAPI ImageProcessThread(LPVOID pParam)
{
	//HSV值得上下限，默认值是蓝色
	CvScalar hsv_min = {105,200,50,0};
	CvScalar hsv_max = {120,255,250,0};

	capture = new cv::VideoCapture("tcp://192.168.1.1:5555"); 
	if(!capture->isOpened())  // check if we succeeded
	{	
		AfxMessageBox("摄像头初始化失败");
		exit(-1);
	}


	IplImage* src_img = NULL;
	IplImage* hsv_img = NULL;
	IplImage* dst_img = NULL;

	for(;;)
	{
		cv::Mat frame;
		*capture >> frame;

		src_img = new IplImage(frame);
		hsv_img = cvCreateImage(cvGetSize(src_img),IPL_DEPTH_8U,3);
		dst_img = cvCreateImage(cvGetSize(src_img),IPL_DEPTH_8U,1);

		cvCvtColor(src_img,hsv_img,CV_BGR2HSV);// BGR -> HSV
		cvInRangeS(hsv_img,hsv_min,hsv_max,dst_img);//HSV值范围检测

		//寻找目标物
		int total_x = 0;
		int total_y = 0;
		int div     = 1;
		int goal_center_x,goal_center_y;

		IplImage* goal_img = new IplImage(*dst_img);

		for (int y=0;y < goal_img->height;y++)
		{
			uchar* ptr = (uchar*)goal_img->imageData + y * goal_img->width;//获得灰度值数据指针
			for (int x=0;x < goal_img->width;x++)
			{
				if(ptr[x] == 255)
				{
					total_x += x;
					total_y += y;
					div     += 1;
				}
			}
		}
		goal_center_x = total_x / div;
		goal_center_y = total_y / div;
		//视频窗口的中心
		int center_x = src_img->width / 2 - 1;
		int center_y = src_img->height / 2 - 1;

		//画出坐标系
		for(int i = 0;i < src_img->width;i++)
			cvSet2D(src_img,center_y,i,cv::Scalar(0,255,0));
		for(int i = 0;i < src_img->height;i++)
			cvSet2D(src_img,i,center_x,cv::Scalar(0,255,0));

		//在视频图像中标出目标的中心点，当特征点过少时，不标识中心点
		if(div > 150)
		{
			//判断目标物的位置
			if(goal_center_x-center_x<-80)
				h = LEFT;
			else if(goal_center_x-center_x>80)
				h = RIGHT;
			else
				h = GET_GOAL;

			if(goal_center_y-center_y<-80)
				v = UP;
			else if(goal_center_y-center_y>80)
				v = DOWN;
			else
				v = GET_GOAL;

			/*控制台输出位于第几象限
			need code
			*/
			printf("-------->目标的中心坐标 (%d,%d)\n",goal_center_x,goal_center_y);
			//在原始图像中标识出中心点
			for(int i = goal_center_x - 5;i < goal_center_x + 6;i++)
			{
				if(i < 0)
					i = 0;
				else if(i >= dst_img->width)
					break;

				cvSet2D(src_img,goal_center_y,i,cv::Scalar(0,0,255));
			}
			for(int i = goal_center_y - 5;i < goal_center_y + 6;i++)
			{
				if(i < 0) 
					i = 0;
				else if(i >= dst_img->height)
					break;

				cvSet2D(src_img,i,goal_center_x,cv::Scalar(0,0,255));
			}
		}
		else
		{
			v = h = NOT_FIND;
			printf("-------->没有找到目标!\n");
		}

		//图像显示
		CvvImage m_CvvImage,m_CvvImage2;
		m_CvvImage.CopyOf(src_img,1);       
		m_CvvImage.DrawToHDC(hDC, &rect);
		m_CvvImage2.CopyOf(dst_img,1);       
		m_CvvImage2.DrawToHDC(hDC2, &rect2); 

		Sleep(30);

		//释放内存
		if(src_img != NULL)
			src_img = NULL;
		if(hsv_img != NULL)
			cvReleaseImage(&hsv_img);
		if(dst_img != NULL)
			cvReleaseImage(&dst_img);
	}
}

//飞行器控制线程
UINT WINAPI ControlThread(LPVOID pParam)
{
	int hover_time = 0;
	char cmd[1024];
	while(1)
	{
		//等待飞行器起飞
		while(drone_state == LANDING && control_mode == NAV_MODE)
		{		
			sprintf_s(cmd, "AT*COMWDG=%d\r", ardrone->getNextSeq());
			ardrone->send_at_cmd(cmd);
			Sleep(40);
			continue;
		}

		//调整面向
		if((ardrone->navData.yaw - goal_yaw) > 5 || (ardrone->navData.yaw - goal_yaw) < -5)
		{
			while((ardrone->navData.yaw - goal_yaw) > 5 || (ardrone->navData.yaw - goal_yaw) < -5)
			{
				if((ardrone->navData.yaw - goal_yaw) > 5)
					ardrone->turnLeft();
				else
					ardrone->turnRight();
				Sleep(5);
			}
			ardrone->hover();
			Sleep(10);
			ardrone->hover();
			Sleep(10);
			ardrone->hover();
			Sleep(10);
		}

		while(1)
		{
			if(h == LEFT)
			{
				ardrone->goingLeft();
				hover_time = 0;
			}
			else if(h == RIGHT)
			{
				ardrone->goingRight();
				hover_time = 0;
			}
			else
			{
				if(v == UP)
				{
					ardrone->goingUp();
					hover_time = 0;
				}
				else if(v == DOWN)
				{
					ardrone->goingDown();
					hover_time = 0;
				}
				else
				{
					ardrone->hover();
					Sleep(5);
					if(v == GET_GOAL&&h == GET_GOAL)
						hover_time++;
					else
						hover_time = 0;
				}
			}
			if(hover_time == 150&&v == GET_GOAL&&h == GET_GOAL)
			{
				for(int i=0;i<200;i++)
				{
					ardrone->goingForward();
					Sleep(10);
				}
				hover_time = 0;
				v = h = NOT_FIND;
				ardrone->hover();
			}
		}
	}
}

//*******************************
CADDRONE_MFCDlg::CADDRONE_MFCDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CADDRONE_MFCDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CADDRONE_MFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_batteryprogress, battery_level_progress);
}

BEGIN_MESSAGE_MAP(CADDRONE_MFCDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CADDRONE_MFCDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CADDRONE_MFCDlg::OnBnClickedCancel)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_startconnection, &CADDRONE_MFCDlg::OnBnClickedstartconnection)
	ON_BN_CLICKED(IDC_closeconnection, &CADDRONE_MFCDlg::OnBnClickedcloseconnection)
	ON_BN_CLICKED(IDC_nav_mode_start, &CADDRONE_MFCDlg::OnBnClickednavmodestart)
	ON_BN_CLICKED(IDC_nav_mode_close, &CADDRONE_MFCDlg::OnBnClickednavmodeclose)
	ON_BN_CLICKED(IDC_keyboard_mode_start, &CADDRONE_MFCDlg::OnBnClickedkeyboardmodestart)
	ON_BN_CLICKED(IDC_keyboard_mode_close, &CADDRONE_MFCDlg::OnBnClickedkeyboardmodeclose)
	ON_BN_CLICKED(IDC_BUTTON1, &CADDRONE_MFCDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// CADDRONE_MFCDlg 消息处理程序

BOOL CADDRONE_MFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	pwnd = GetDlgItem(IDC_srcImg);   
	pDC =pwnd->GetDC();   
	hDC = pDC->GetSafeHdc();  
	pwnd->GetClientRect(&rect); 

	pwnd2 = GetDlgItem(IDC_dstImg);   
	pDC2 =pwnd2->GetDC();   
	hDC2 = pDC2->GetSafeHdc();  
	pwnd2->GetClientRect(&rect2);

	battery_level_progress.SetRange(0,100);
	control_mode = DISCONNECT_MODE;

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CADDRONE_MFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CADDRONE_MFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CADDRONE_MFCDlg::OnBnClickedOk()
{
	// TODO: 在此添加控件通知处理程序代码 
	CDialogEx::OnOK();
}


void CADDRONE_MFCDlg::OnBnClickedCancel()
{
	// TODO: 在此添加控件通知处理程序代码
	CDialogEx::OnCancel();
}


void CADDRONE_MFCDlg::OnTimer(UINT_PTR nIDEvent)
{
	cv::Mat frame; 
	CvvImage m_CvvImage,m_CvvImage2;
	IplImage* src_img = NULL;
	IplImage* hsv_img = NULL;
	IplImage* dst_img = NULL;
	cv::Mat medianblur_img;
	IplImage* goal_img;

	int total_x = 0;
	int total_y = 0;
	int div     = 1;
	int goal_center_x,goal_center_y;

	int center_x,center_y;
	char text[100] = { 0 };

	switch(nIDEvent)
	{
	case 1:
		*capture >> frame;

		src_img = new IplImage(frame);
		hsv_img = cvCreateImage(cvGetSize(src_img),IPL_DEPTH_8U,3);
		dst_img = cvCreateImage(cvGetSize(src_img),IPL_DEPTH_8U,1);

		if(control_mode == NAV_MODE)
		{
			cvCvtColor(src_img,hsv_img,CV_BGR2HSV);// BGR -> HSV
			cvInRangeS(hsv_img,hsv_min,hsv_max,dst_img);//HSV值范围检测

			//寻找目标物
			goal_img = dst_img;
			for (int y=0;y < goal_img->height;y++)
			{
				uchar* ptr = (uchar*)goal_img->imageData + y * dst_img->width;//获得灰度值数据指针
				for (int x=0;x < goal_img->width;x++)
				{
					if(ptr[x] == 255)
					{
						total_x += x;
						total_y += y;
						div     += 1;
					}
				}
			}
			goal_center_x = total_x / div;
			goal_center_y = total_y / div;

			////画出坐标系
			int center_area_width = 50;
			int center_area_height = 60;

			center_x = src_img->width / 2 - 1;
			center_y = src_img->height / 2 - 1;

			for(int i = 0;i < src_img->width;i++)
				cvSet2D(src_img,center_y,i,cv::Scalar(0,255,0));
			for(int i = 0;i < src_img->height;i++)
				cvSet2D(src_img,i,center_x,cv::Scalar(0,255,0));

			//画出中心区域
			for(int i = center_x -(center_area_width / 2);i < center_x +(center_area_width / 2);i++)
			{
				cvSet2D(src_img,center_y - (center_area_height / 2),i,cv::Scalar(0,255,0));
				cvSet2D(src_img,center_y + (center_area_height / 2),i,cv::Scalar(0,255,0));
			}
			for(int i = center_y - (center_area_height / 2);i < center_y + (center_area_height / 2);i++)
			{
				cvSet2D(src_img,i,center_x - (center_area_width / 2),cv::Scalar(0,255,0));
				cvSet2D(src_img,i,center_x + (center_area_width / 2),cv::Scalar(0,255,0));
			}

			//在视频图像中标出目标的中心点，当特征点过少时，不标识中心点
			if(div > 300)
			{
				char text[100] = { 0 };
				sprintf_s(text,"目标的中心坐标 (%d,%d)",goal_center_x,goal_center_y);
				AppendText(IDC_goal_pos,text);
				//判断目标物的位置
				if(goal_center_x-center_x < -(center_area_width / 2))
					h = LEFT;
				else if(goal_center_x-center_x > (center_area_width / 2))
					h = RIGHT;
				else
					h = GET_GOAL;

				if(goal_center_y-center_y < -(center_area_height / 2 - 10))
					v = UP;
				else if(goal_center_y-center_y > (center_area_height / 2 + 10))
					v = DOWN;
				else
					v = GET_GOAL;
				//在原始图像中标识出中心点
				for(int i = goal_center_x - 5;i < goal_center_x + 6;i++)
				{
					if(i < 0)
						i = 0;
					else if(i >= dst_img->width)
						break;

					cvSet2D(src_img,goal_center_y,i,cv::Scalar(0,0,255));
				}
				for(int i = goal_center_y - 5;i < goal_center_y + 6;i++)
				{
					if(i < 0) 
						i = 0;
					else if(i >= dst_img->height)
						break;

					cvSet2D(src_img,i,goal_center_x,cv::Scalar(0,0,255));
				}
			}
			else
			{
				v = h = NOT_FIND;
				AppendText(IDC_goal_pos,"没有找到目标!\r");
			}
		}

		m_CvvImage.CopyOf(src_img,1);       
		m_CvvImage.DrawToHDC(hDC, &rect);
		m_CvvImage2.CopyOf(dst_img,1);       
		m_CvvImage2.DrawToHDC(hDC2, &rect2); 

		if(src_img != NULL)
			src_img = NULL;
		if(hsv_img != NULL)
			cvReleaseImage(&hsv_img);
		if(dst_img != NULL)
			cvReleaseImage(&dst_img);
		break;
	case 2:
		sprintf_s(text,"%d",ardrone->navData.batteryLevel);
		GetDlgItem(IDC_batterylevel)->SetWindowTextA(text);
		battery_level_progress.SetPos(ardrone->navData.batteryLevel);
		sprintf_s(text,"%d",ardrone->navData.pitch);
		GetDlgItem(IDC_pitch)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.roll);
		GetDlgItem(IDC_roll)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.yaw);
		GetDlgItem(IDC_yaw)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.altitude);
		GetDlgItem(IDC_altitude)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.vx);
		GetDlgItem(IDC_vx)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.vy);
		GetDlgItem(IDC_vy)->SetWindowTextA(text);
		sprintf_s(text,"%d",ardrone->navData.vz);
		GetDlgItem(IDC_vz)->SetWindowTextA(text);
		break;
	case 3:
		AppendText(IDC_control_cmd,ardrone->at_cmd_last);
		break;
	}
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	CDialogEx::OnTimer(nIDEvent);
}


void CADDRONE_MFCDlg::OnBnClickedstartconnection()
{
	// TODO: 在此添加控件通知处理程序代码
	if(control_mode != DISCONNECT_MODE) return;
	ardrone = new ARDrone("myardrone");
	AppendText(IDC_control_cmd,"连接飞行器...完成");
	wakeup_thread = AfxBeginThread((AFX_THREADPROC)WakeUpThread,NULL);
	if(wakeup_thread == NULL)
	{ 
		AppendText(IDC_control_cmd,"连接保持线程初始化...失败");
		return;
	}
	AppendText(IDC_control_cmd,"连接保持线程初始化...完成");
	navdata_thread = AfxBeginThread((AFX_THREADPROC)NavDataThread,NULL);
	if(navdata_thread == NULL)
	{ 
		AppendText(IDC_control_cmd,"导航数据接收线程初始化...失败");
		return;
	}
	AppendText(IDC_control_cmd,"导航数据接收线程初始化...完成");
	//ardrone->initializeCmd();
	AppendText(IDC_control_cmd,"飞行器初始化...完成");
	//设置控制模式
	control_mode = CONNECT_MODE;

	control_thread = AfxBeginThread((AFX_THREADPROC)ControlThread,NULL);

	AppendText(IDC_control_cmd,"当前控制模式为：仅连接");
	capture = new cv::VideoCapture("tcp://192.168.1.1:5555"); 
	if(!capture->isOpened())
	{	
		AfxMessageBox("摄像头初始化失败");
		return;
	}
	AppendText(IDC_control_cmd,"摄像头初始化...完成");
	//设置计时器  
	SetTimer(1,30,NULL);
	SetTimer(2,5,NULL);
}


void CADDRONE_MFCDlg::OnBnClickedcloseconnection()
{
	// TODO: 在此添加控件通知处理程序代码
	control_mode = DISCONNECT_MODE;
}


void CADDRONE_MFCDlg::OnBnClickednavmodestart()
{
	// TODO: 在此添加控件通知处理程序代码
	if(control_mode == DISCONNECT_MODE)
	{
		AfxMessageBox("请先连接飞行器！");
		return;
	}
	if(control_mode == KEYBOARD_MODE)
	{
		AfxMessageBox("请先关闭键盘控制模式！");
		return;
	}

	control_mode = NAV_MODE;
	SetTimer(3,5,NULL);
}


void CADDRONE_MFCDlg::OnBnClickednavmodeclose()
{
	// TODO: 在此添加控件通知处理程序代码
	if(control_mode != NAV_MODE) return;
	control_mode = CONNECT_MODE;
	KillTimer(3);
	ClearText(IDC_goal_pos);
	ClearText(IDC_control_cmd);
}


void CADDRONE_MFCDlg::OnBnClickedkeyboardmodestart()
{
	// TODO: 在此添加控件通知处理程序代码
	if(control_mode == DISCONNECT_MODE)
	{
		AfxMessageBox("请先连接飞行器！");
		return;
	}
	if(control_mode == NAV_MODE)
	{
		AfxMessageBox("请先关闭导航模式！");
		return;
	}
	control_mode = KEYBOARD_MODE;
}


void CADDRONE_MFCDlg::OnBnClickedkeyboardmodeclose()
{
	// TODO: 在此添加控件通知处理程序代码
	if(control_mode != KEYBOARD_MODE) return;
	ardrone->land();
	control_mode = CONNECT_MODE;
	ClearText(IDC_goal_pos);
	ClearText(IDC_control_cmd);
}


BOOL CADDRONE_MFCDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类
	if(control_mode == KEYBOARD_MODE)
	{
		if(pMsg->message==WM_KEYDOWN)
		{
			UINT key = pMsg->wParam;
			switch(key)
			{
			case 49://1
				ardrone->takeoff();
				drone_state = FLYING;
				AppendText(IDC_control_cmd,"---->起飞");
				break;
			case 50://2
				ardrone->land();
				drone_state = LANDING;
				AppendText(IDC_control_cmd,"---->降落");
				break;
			case 73://i
				ardrone->goingUp();
				AppendText(IDC_control_cmd,"---->向上");
				break;
			case 75://k
				ardrone->goingDown();
				AppendText(IDC_control_cmd,"---->向下");
				break;
			case 74://j
				ardrone->goingLeft();
				AppendText(IDC_control_cmd,"---->左移");
				break;
			case 76://l
				ardrone->goingRight();
				AppendText(IDC_control_cmd,"---->右移");
				break;
			case 72://h
				ardrone->hover();
				AppendText(IDC_control_cmd,"---->悬停");
				break;
			case 85://u
				ardrone->turnLeft();
				AppendText(IDC_control_cmd,"---->左转");
				break;
			case 79://o
				ardrone->turnRight();
				AppendText(IDC_control_cmd,"---->右转");
				break;
			case 87://w
				ardrone->goingForward();
				AppendText(IDC_control_cmd,"---->前进");
				break;
			case 83://s
				ardrone->goingBack();
				AppendText(IDC_control_cmd,"---->后退");
				break;
			default:
				//char text[10] = {0};
				//sprintf_s(text,"%d",key);
				AppendText(IDC_control_cmd,"未知操作！！！");
				break;
			}
		}
	}
	else if(control_mode == NAV_MODE)
	{
		if(pMsg->message==WM_KEYDOWN)
		{
			UINT key = pMsg->wParam;
			switch(key)
			{
			case 49://1
				ardrone->takeoff();
				drone_state = FLYING;
				AppendText(IDC_control_cmd,"---->起飞");
				break;
			case 50://2
				ardrone->land();
				drone_state = LANDING;
				AppendText(IDC_control_cmd,"---->降落");
				break;
			}
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CADDRONE_MFCDlg::OnBnClickedButton1()
{
	// TODO: 在此添加控件通知处理程序代码
	cv::Mat frame;
	*capture >> frame;
	IplImage* img = new IplImage(frame);
	cvSaveImage("sample.jpg",img);
	CDlgColorCollect DlgCC;
	DlgCC.DoModal();
}

//追加文本到EditControl
void CADDRONE_MFCDlg::AppendText(int controlId, CString strAdd)
{
	if(GetDlgItem(controlId)->GetWindowTextLength() > 10240)
		((CEdit*)GetDlgItem(controlId))->SetWindowTextA("");
	((CEdit*)GetDlgItem(controlId))->SetSel(GetDlgItem(controlId)->GetWindowTextLength(),GetDlgItem(controlId)->GetWindowTextLength());
	((CEdit*)GetDlgItem(controlId))->ReplaceSel(strAdd+L"\n");
}

//清空EditControl
void CADDRONE_MFCDlg::ClearText(int controlId)
{
	((CEdit*)GetDlgItem(controlId))->SetWindowTextA("");
}
