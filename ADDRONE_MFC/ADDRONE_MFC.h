
// ADDRONE_MFC.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������
#include "CvvImage.h"
#include <opencv2\opencv.hpp>

// CADDRONE_MFCApp:
// �йش����ʵ�֣������ ADDRONE_MFC.cpp
//

class CADDRONE_MFCApp : public CWinApp
{
public:
	CADDRONE_MFCApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CADDRONE_MFCApp theApp;