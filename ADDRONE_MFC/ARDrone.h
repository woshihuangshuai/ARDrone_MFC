#pragma once

#include <mutex>
#include <thread>
#include "MemoryLibrary.h"

/*************************************************
* 常量声明定义模块
**************************************************/
// three Port
const int  NAVDATA_PORT = 5554;
const int  VIDEO_PORT   = 5555;
const int  AT_PORT      = 5556;

const int INTERVAL = 100;
static char* ARDrone_IP = "192.168.1.1";
const int C_OK = 1;
const int C_ERRO = 0;

// 联合体：Buffer
union INT_FLOAT_BUFFER
{
	float	fBuff;
	int		iBuff;
};

// Navdata Struct
struct NAV_DATA
{
	int32_t header;
	int32_t state;
	int32_t sequence;
	int32_t visionDefined;
	int16_t tag;
	int16_t size;
	int32_t ctrlState;
	int32_t batteryLevel;
	int32_t pitch;
	int32_t roll;
	int32_t yaw;
	int32_t altitude;
	int32_t vx;
	int32_t vy;
	int32_t vz;
};


/*
* ARDrone 自定义类的详细介绍
*
*【控制操作】
* 已完成：起飞、降落、前进、后退、向左飞、向右飞、调整速度
*【成员变量操作】
* 已完成：获取当前序号、获取前序号、获取后序号、设置前序号
*【发送指令】
* 发送基础指令、发送飞行控制指令
*【初始化操作】
* 发送飞行器初始化配置指令、socket初始化操作
*【辅助分析】
* 已完成：数据包分析、float类型转换int类型
* 待完成：无
*【公有成员变量】
* 导航数据、前一个指令
*【私有成员变量】
* 发送指令的套接字（一套）、飞行速度、飞行器名字、包序号、前包序号、互斥锁
*
* 原则：有所为，有所不为
*/
class ARDrone
{
public:
	ARDrone(void){}
	ARDrone(char*);
	~ARDrone(void);

public:
	// 飞行器操作
	void switchtoFrontCamera();
	void switchtoDownCamera();
	void takeoff();
	void land();
	void hover();

	void goingUp();
	void goingDown();
	void goingForward();
	void goingBack();
	void goingLeft();
	void goingRight();

	void turnLeft();
	void turnRight();

	void setSpeed(int);		// 设置飞行速度

public:
	int		getCurrentSeq(){ return this->seq_; }			// get the current sequence
	int		getLastSeq(){ return this->lastSeq_; }		// get the last sequence
	int		getNextSeq();								// get the next sequence
	void	setLastSeq(int);							// set the last sequence

	int		send_at_cmd(char*);					// send all command
	int		send_pcmd(int, float, float, float, float);	// send control command
	void	parse(MemoryLibrary::Buffer&);				// 数据包分析
	void	initializeCmd();				// initialize command

protected:
	void	initializeSocketaddr();			// initialize sockaddr_in
	int		floatToInt(float);				// 使用联合体实现float 转化int	

public:
	NAV_DATA		navData;			// ardrone's navdata 
	char*			at_cmd_last;		// save the last command

private:
	SOCKET		socketat_;		// socket
	sockaddr_in Atsin_;			// struct
	int			lenSin_;		// the length of sin
	float		speed_;			// fly speed
	char*		name_;			// ardrone's name
	int			seq_;			// sequence of data packet 
	int			lastSeq_;		// save the last seq
	std::mutex	mtx;			// mutex for critical section
};