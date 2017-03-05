
// ADDRONE_MFCDlg.cpp : ʵ���ļ�
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

//����ģʽ
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
int v = NOT_FIND;	//0,+1,-1,��ֱ����
int h = NOT_FIND;	//0,+1,-1��ˮƽ����

//ͼ����ʾ
CRect rect;  
CRect rect2;
CDC *pDC ,*pDC2;  
HDC hDC,hDC2;  
CWnd *pwnd,*pwnd2; 

//ͼ������ر���
//CvScalar hsv_min = {105,200,50,0};
//CvScalar hsv_max = {120,255,250,0};

////��ɫ
//CvScalar hsv_min = {105,150,100,0};
//CvScalar hsv_max = {120,255,200,0};

//��ɫ
CvScalar hsv_min = {120,100,100,0};
CvScalar hsv_max = {190,255,255,0};

ARDrone *ardrone;
cv::VideoCapture *capture;

/*********************�����߳�***********************/
CWinThread* wakeup_thread = NULL;
CWinThread* navdata_thread = NULL;
CWinThread* control_thread = NULL;

// ������Ardrone���ӵ��߳�
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

// ��ȡ�������ݵ��߳�
UINT WINAPI NavDataThread(LPVOID pParam)
{
	SOCKET socketNav_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in	navSin_;
	navSin_.sin_family = AF_INET;
	navSin_.sin_port = htons(NAVDATA_PORT);
	navSin_.sin_addr.s_addr = inet_addr(ARDrone_IP);
	int lenNavSin_ = sizeof(navSin_);

	// ����ָ���NAVDATA_PORT �˿�
	const char trigger[4] = { 0x01, 0x00, 0x00, 0x00 };
	int result = sendto(socketNav_, trigger, strlen(trigger), 0, (sockaddr *)&navSin_, lenNavSin_);
	if (result != SOCKET_ERROR)
	{
		printf_s("Sent trigger flag to UDP port : %d \n", NAVDATA_PORT);
	}

	// ����ָ���AT_PORT �˿�
	char initCmd[1024] = { 0 };
	sprintf_s(initCmd, "AT*CONFIG=%d,\"general:navdata_demo\",\"TRUE\"\r", ardrone->getNextSeq());
	assert(ardrone->send_at_cmd(initCmd));

	// �������ݰ�
	MemoryLibrary::Buffer navDataBuffer;	// ���������ݻ�����
	char recv[1024] = { 0 };				// ���ݰ���������
	int lenRecv = 0;
	int delay = 0;
	// UI ����
	char text[100] = { 0 };					// ����������ʾ�ַ���

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

// ��Ƶ�����߳�
UINT WINAPI ImageProcessThread(LPVOID pParam)
{
	//HSVֵ�������ޣ�Ĭ��ֵ����ɫ
	CvScalar hsv_min = {105,200,50,0};
	CvScalar hsv_max = {120,255,250,0};

	capture = new cv::VideoCapture("tcp://192.168.1.1:5555"); 
	if(!capture->isOpened())  // check if we succeeded
	{	
		AfxMessageBox("����ͷ��ʼ��ʧ��");
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
		cvInRangeS(hsv_img,hsv_min,hsv_max,dst_img);//HSVֵ��Χ���

		//Ѱ��Ŀ����
		int total_x = 0;
		int total_y = 0;
		int div     = 1;
		int goal_center_x,goal_center_y;

		IplImage* goal_img = new IplImage(*dst_img);

		for (int y=0;y < goal_img->height;y++)
		{
			uchar* ptr = (uchar*)goal_img->imageData + y * goal_img->width;//��ûҶ�ֵ����ָ��
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
		//��Ƶ���ڵ�����
		int center_x = src_img->width / 2 - 1;
		int center_y = src_img->height / 2 - 1;

		//��������ϵ
		for(int i = 0;i < src_img->width;i++)
			cvSet2D(src_img,center_y,i,cv::Scalar(0,255,0));
		for(int i = 0;i < src_img->height;i++)
			cvSet2D(src_img,i,center_x,cv::Scalar(0,255,0));

		//����Ƶͼ���б��Ŀ������ĵ㣬�����������ʱ������ʶ���ĵ�
		if(div > 150)
		{
			//�ж�Ŀ�����λ��
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

			/*����̨���λ�ڵڼ�����
			need code
			*/
			printf("-------->Ŀ����������� (%d,%d)\n",goal_center_x,goal_center_y);
			//��ԭʼͼ���б�ʶ�����ĵ�
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
			printf("-------->û���ҵ�Ŀ��!\n");
		}

		//ͼ����ʾ
		CvvImage m_CvvImage,m_CvvImage2;
		m_CvvImage.CopyOf(src_img,1);       
		m_CvvImage.DrawToHDC(hDC, &rect);
		m_CvvImage2.CopyOf(dst_img,1);       
		m_CvvImage2.DrawToHDC(hDC2, &rect2); 

		Sleep(30);

		//�ͷ��ڴ�
		if(src_img != NULL)
			src_img = NULL;
		if(hsv_img != NULL)
			cvReleaseImage(&hsv_img);
		if(dst_img != NULL)
			cvReleaseImage(&dst_img);
	}
}

//�����������߳�
UINT WINAPI ControlThread(LPVOID pParam)
{
	int hover_time = 0;
	char cmd[1024];
	while(1)
	{
		//�ȴ����������
		while(drone_state == LANDING && control_mode == NAV_MODE)
		{		
			sprintf_s(cmd, "AT*COMWDG=%d\r", ardrone->getNextSeq());
			ardrone->send_at_cmd(cmd);
			Sleep(40);
			continue;
		}

		//��������
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


// CADDRONE_MFCDlg ��Ϣ�������

BOOL CADDRONE_MFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
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

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CADDRONE_MFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CADDRONE_MFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CADDRONE_MFCDlg::OnBnClickedOk()
{
	// TODO: �ڴ���ӿؼ�֪ͨ���������� 
	CDialogEx::OnOK();
}


void CADDRONE_MFCDlg::OnBnClickedCancel()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
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
			cvInRangeS(hsv_img,hsv_min,hsv_max,dst_img);//HSVֵ��Χ���

			//Ѱ��Ŀ����
			goal_img = dst_img;
			for (int y=0;y < goal_img->height;y++)
			{
				uchar* ptr = (uchar*)goal_img->imageData + y * dst_img->width;//��ûҶ�ֵ����ָ��
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

			////��������ϵ
			int center_area_width = 50;
			int center_area_height = 60;

			center_x = src_img->width / 2 - 1;
			center_y = src_img->height / 2 - 1;

			for(int i = 0;i < src_img->width;i++)
				cvSet2D(src_img,center_y,i,cv::Scalar(0,255,0));
			for(int i = 0;i < src_img->height;i++)
				cvSet2D(src_img,i,center_x,cv::Scalar(0,255,0));

			//������������
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

			//����Ƶͼ���б��Ŀ������ĵ㣬�����������ʱ������ʶ���ĵ�
			if(div > 300)
			{
				char text[100] = { 0 };
				sprintf_s(text,"Ŀ����������� (%d,%d)",goal_center_x,goal_center_y);
				AppendText(IDC_goal_pos,text);
				//�ж�Ŀ�����λ��
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
				//��ԭʼͼ���б�ʶ�����ĵ�
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
				AppendText(IDC_goal_pos,"û���ҵ�Ŀ��!\r");
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
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	CDialogEx::OnTimer(nIDEvent);
}


void CADDRONE_MFCDlg::OnBnClickedstartconnection()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	if(control_mode != DISCONNECT_MODE) return;
	ardrone = new ARDrone("myardrone");
	AppendText(IDC_control_cmd,"���ӷ�����...���");
	wakeup_thread = AfxBeginThread((AFX_THREADPROC)WakeUpThread,NULL);
	if(wakeup_thread == NULL)
	{ 
		AppendText(IDC_control_cmd,"���ӱ����̳߳�ʼ��...ʧ��");
		return;
	}
	AppendText(IDC_control_cmd,"���ӱ����̳߳�ʼ��...���");
	navdata_thread = AfxBeginThread((AFX_THREADPROC)NavDataThread,NULL);
	if(navdata_thread == NULL)
	{ 
		AppendText(IDC_control_cmd,"�������ݽ����̳߳�ʼ��...ʧ��");
		return;
	}
	AppendText(IDC_control_cmd,"�������ݽ����̳߳�ʼ��...���");
	//ardrone->initializeCmd();
	AppendText(IDC_control_cmd,"��������ʼ��...���");
	//���ÿ���ģʽ
	control_mode = CONNECT_MODE;

	control_thread = AfxBeginThread((AFX_THREADPROC)ControlThread,NULL);

	AppendText(IDC_control_cmd,"��ǰ����ģʽΪ��������");
	capture = new cv::VideoCapture("tcp://192.168.1.1:5555"); 
	if(!capture->isOpened())
	{	
		AfxMessageBox("����ͷ��ʼ��ʧ��");
		return;
	}
	AppendText(IDC_control_cmd,"����ͷ��ʼ��...���");
	//���ü�ʱ��  
	SetTimer(1,30,NULL);
	SetTimer(2,5,NULL);
}


void CADDRONE_MFCDlg::OnBnClickedcloseconnection()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	control_mode = DISCONNECT_MODE;
}


void CADDRONE_MFCDlg::OnBnClickednavmodestart()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	if(control_mode == DISCONNECT_MODE)
	{
		AfxMessageBox("�������ӷ�������");
		return;
	}
	if(control_mode == KEYBOARD_MODE)
	{
		AfxMessageBox("���ȹرռ��̿���ģʽ��");
		return;
	}

	control_mode = NAV_MODE;
	SetTimer(3,5,NULL);
}


void CADDRONE_MFCDlg::OnBnClickednavmodeclose()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	if(control_mode != NAV_MODE) return;
	control_mode = CONNECT_MODE;
	KillTimer(3);
	ClearText(IDC_goal_pos);
	ClearText(IDC_control_cmd);
}


void CADDRONE_MFCDlg::OnBnClickedkeyboardmodestart()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	if(control_mode == DISCONNECT_MODE)
	{
		AfxMessageBox("�������ӷ�������");
		return;
	}
	if(control_mode == NAV_MODE)
	{
		AfxMessageBox("���ȹرյ���ģʽ��");
		return;
	}
	control_mode = KEYBOARD_MODE;
}


void CADDRONE_MFCDlg::OnBnClickedkeyboardmodeclose()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	if(control_mode != KEYBOARD_MODE) return;
	ardrone->land();
	control_mode = CONNECT_MODE;
	ClearText(IDC_goal_pos);
	ClearText(IDC_control_cmd);
}


BOOL CADDRONE_MFCDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: �ڴ����ר�ô����/����û���
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
				AppendText(IDC_control_cmd,"---->���");
				break;
			case 50://2
				ardrone->land();
				drone_state = LANDING;
				AppendText(IDC_control_cmd,"---->����");
				break;
			case 73://i
				ardrone->goingUp();
				AppendText(IDC_control_cmd,"---->����");
				break;
			case 75://k
				ardrone->goingDown();
				AppendText(IDC_control_cmd,"---->����");
				break;
			case 74://j
				ardrone->goingLeft();
				AppendText(IDC_control_cmd,"---->����");
				break;
			case 76://l
				ardrone->goingRight();
				AppendText(IDC_control_cmd,"---->����");
				break;
			case 72://h
				ardrone->hover();
				AppendText(IDC_control_cmd,"---->��ͣ");
				break;
			case 85://u
				ardrone->turnLeft();
				AppendText(IDC_control_cmd,"---->��ת");
				break;
			case 79://o
				ardrone->turnRight();
				AppendText(IDC_control_cmd,"---->��ת");
				break;
			case 87://w
				ardrone->goingForward();
				AppendText(IDC_control_cmd,"---->ǰ��");
				break;
			case 83://s
				ardrone->goingBack();
				AppendText(IDC_control_cmd,"---->����");
				break;
			default:
				//char text[10] = {0};
				//sprintf_s(text,"%d",key);
				AppendText(IDC_control_cmd,"δ֪����������");
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
				AppendText(IDC_control_cmd,"---->���");
				break;
			case 50://2
				ardrone->land();
				drone_state = LANDING;
				AppendText(IDC_control_cmd,"---->����");
				break;
			}
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CADDRONE_MFCDlg::OnBnClickedButton1()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	cv::Mat frame;
	*capture >> frame;
	IplImage* img = new IplImage(frame);
	cvSaveImage("sample.jpg",img);
	CDlgColorCollect DlgCC;
	DlgCC.DoModal();
}

//׷���ı���EditControl
void CADDRONE_MFCDlg::AppendText(int controlId, CString strAdd)
{
	if(GetDlgItem(controlId)->GetWindowTextLength() > 10240)
		((CEdit*)GetDlgItem(controlId))->SetWindowTextA("");
	((CEdit*)GetDlgItem(controlId))->SetSel(GetDlgItem(controlId)->GetWindowTextLength(),GetDlgItem(controlId)->GetWindowTextLength());
	((CEdit*)GetDlgItem(controlId))->ReplaceSel(strAdd+L"\n");
}

//���EditControl
void CADDRONE_MFCDlg::ClearText(int controlId)
{
	((CEdit*)GetDlgItem(controlId))->SetWindowTextA("");
}
