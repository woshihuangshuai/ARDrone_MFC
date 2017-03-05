#include "stdafx.h"

#include "ARDrone.h"
#include <stdio.h>  
#include <winsock2.h>  
#include <assert.h>
#include <stdint.h>

#include "MemoryLibrary.h"
#pragma comment(lib, "ws2_32.lib") 

using namespace std;

/*************************************************
* ARDrone 类成员函数的实现模块（model）
**************************************************/
ARDrone::ARDrone(char* name)
{
	this->name_ = name;
	// 用WSAStartup 启动Ws2_32.lib
	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	assert(WSAStartup(socketVersion, &wsaData) == 0);

	// 成员变量初始化
	this->socketat_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	initializeSocketaddr();

	lenSin_		= sizeof(Atsin_);
	speed_		= 0.2f;
	seq_		= 1;
	lastSeq_	= 1;
	at_cmd_last = new char[1024];
}

// Destructors
ARDrone::~ARDrone()
{
	delete[] at_cmd_last;
	delete[] name_;

	WSACleanup();					// 释放Winsock库
	closesocket(this->socketat_);	// 关闭SOCKET
}

// initialize sockaddr_in
void ARDrone::initializeSocketaddr()
{
	Atsin_.sin_family = AF_INET;
	Atsin_.sin_port = htons(AT_PORT);
	Atsin_.sin_addr.s_addr = inet_addr(ARDrone_IP);
	//cout << "IP:" << ARDrone_IP << "Port:" << AT_PORT << std::endl;
}

// initialize command
void ARDrone::initializeCmd()
{
	char cmd[1024];
	//// 设置最大高度
	//sprintf_s(cmd, "AT*CONFIG=%d,\"control:altitude_max\",\"4572\"\r", getNextSeq());
	//assert(send_at_cmd(cmd));
	//Sleep(5);

	//设置有无防护罩
	//sprintf_s(cmd, "AT*CONFIG=%d,\"control:flight_without_shell\",\"TRUE\"\r", getNextSeq());
	//assert(send_at_cmd(cmd));
	//Sleep(5);

	//设置控制等级
	sprintf_s(cmd, "AT*CONFIG=%d,\"control:control_level\",\"2\"\r", getNextSeq());
	assert(send_at_cmd(cmd));
	Sleep(5);

	////// 设置超声波频率
	//sprintf_s(cmd, "AT*CONFIG=%d,\"pic:ultrasound_freq\",\"8\"\r", getNextSeq());
	//assert(send_at_cmd(cmd));
	//Sleep(5);

	//////flat trim
	//sprintf_s(cmd, "AT*FTRIM=%d\r", getNextSeq());
	//assert(send_at_cmd(cmd));
	//Sleep(5);
}

// 飞行控制函数实现部分
void ARDrone::switchtoFrontCamera()
{
	char cmd[100];
	sprintf_s(cmd,"AT*CONFIG=%d,\"video:video_channel\",\"0\"\r",getNextSeq());
	assert(send_at_cmd(cmd));
}

void ARDrone::switchtoDownCamera()
{
	char cmd[100];
	sprintf_s(cmd,"AT*CONFIG=%d,\"video:video_channel\",\"1\"\r",getNextSeq());
	assert(send_at_cmd(cmd));
}

void ARDrone::takeoff()
{
	char cmd[1024];
	//sprintf_s(cmd,"AT*FTRIM%d\r",getNextSeq());
	//assert(send_at_cmd(cmd));
	sprintf_s(cmd, "AT*REF=%d,290718208\r", getNextSeq());
	assert(send_at_cmd(cmd));
	printf("take off\n");
}

void ARDrone::land()
{
	char cmd[100];
	sprintf_s(cmd, "AT*REF=%d,290717696\r", getNextSeq());
	assert(send_at_cmd(cmd));
	printf("land\n");
}

void ARDrone::hover()
{
	assert(send_pcmd(0, 0, 0, 0, 0));
	printf("Hover\n");
}

void ARDrone::goingUp()
{
	assert(send_pcmd(1, 0, 0, +0.2f, 0));
	printf("goingUp\n");
}

void ARDrone::goingDown()
{
	assert(send_pcmd(1, 0, 0, -0.2f, 0));
	printf("goingDown\n");
}

void ARDrone::goingForward()
{
	assert(send_pcmd(1, 0, -speed_, 0, 0));
}

void ARDrone::goingBack()
{
	assert(send_pcmd(1, 0, +speed_, 0, 0));
}
void ARDrone::goingLeft()
{
	assert(send_pcmd(1, -0.05f, 0, 0, 0));
	printf("goingLeft\n");
}

void ARDrone::goingRight()
{
	assert(send_pcmd(1, +0.05f, 0, 0, 0));
	printf("goingRight\n");
}

void ARDrone::turnLeft()
{
	assert(send_pcmd(1, 0, 0, 0, -0.3f));
	printf("turn Left\n");
}

void ARDrone::turnRight()
{
	assert(send_pcmd(1, 0, 0, 0, +0.3f));
	printf("turnRight\n");
}

void ARDrone::setSpeed(int mul)
{
	this->speed_ = mul * 0.1f;
}

// get the lastest sequence
int ARDrone::getNextSeq()
{
	// 互斥锁:用于多线程
	this->mtx.lock();
	seq_ += 1;
	this->mtx.unlock();
	return seq_;
}

//set the last sequence
void ARDrone::setLastSeq(int currentSeq)
{
	this->lastSeq_ = currentSeq;
}

// send control command
int ARDrone::send_pcmd(int enable, float roll, float pitch, float gaz, float yaw)
{
	char cmd[1024];
	sprintf_s(cmd, "AT*PCMD=%d,%d,%d,%d,%d,%d\r", getNextSeq(), enable,
		floatToInt(roll), floatToInt(pitch), floatToInt(gaz), floatToInt(yaw));
	int result = send_at_cmd(cmd);
	return result;
}

// send all command
int ARDrone::send_at_cmd(char* cmd)
{
	// 互斥锁:用于多线程C++11
	this->mtx.lock();
	at_cmd_last = cmd;
	int result = sendto(this->socketat_, cmd, strlen(cmd), 0, (sockaddr *)&Atsin_, this->lenSin_);
	if (result == SOCKET_ERROR)
		return C_ERRO;

	this->mtx.unlock();
	return C_OK;
}

// get the same memory of float
int ARDrone::floatToInt(float f)
{
	int result;
	memcpy(&result, &f, sizeof(int));
	return result;
}

// parse data packet
void ARDrone::parse(MemoryLibrary::Buffer& buffer)
{
	int offset = 0;
	int header = buffer.MakeValueFromOffset<int32_t>(offset);
	if (header != 0x55667788)
	{
		//cout << "NavigationDataReceiver FAIL, because the header != 0x55667788\n";
		return;
	}

	offset += 4;
	int state = buffer.MakeValueFromOffset<int32_t>(offset);
	navData.state = state;

	offset += 4;
	int sequence = buffer.MakeValueFromOffset<int32_t>(offset);
	navData.sequence = sequence;

	offset += 4;
	int visionDefined = buffer.MakeValueFromOffset<int32_t>(offset);
	navData.visionDefined = visionDefined;

	offset += 4;
	int16_t tag = buffer.MakeValueFromOffset<int16_t>(offset);
	navData.tag = tag;

	offset += 2;
	int16_t size = buffer.MakeValueFromOffset<int16_t>(offset);
	navData.size = size;

	offset += 2;
	int ctrlState = buffer.MakeValueFromOffset<int32_t>(offset);
	navData.ctrlState = ctrlState;

	offset += 4;
	navData.batteryLevel =  buffer.MakeValueFromOffset<int>(offset);

	offset += 4;
	navData.pitch = buffer.MakeValueFromOffset<float>(offset) / 1000.0f;

	offset += 4;
	navData.roll = buffer.MakeValueFromOffset<float>(offset) / 1000.0f;

	offset += 4;
	navData.yaw = buffer.MakeValueFromOffset<float>(offset) / 1000.0f;

	offset += 4;
	navData.altitude = (float)buffer.MakeValueFromOffset<int>(offset) / 1000.0f;

	offset += 4;
	navData.vx = buffer.MakeValueFromOffset<float>(offset);

	offset += 4;
	navData.vy = buffer.MakeValueFromOffset<float>(offset);

	offset += 4;
	navData.vz = buffer.MakeValueFromOffset<float>(offset);

	offset += 4;
}