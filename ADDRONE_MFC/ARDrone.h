#pragma once

#include <mutex>
#include <thread>
#include "MemoryLibrary.h"

/*************************************************
* ������������ģ��
**************************************************/
// three Port
const int  NAVDATA_PORT = 5554;
const int  VIDEO_PORT   = 5555;
const int  AT_PORT      = 5556;

const int INTERVAL = 100;
static char* ARDrone_IP = "192.168.1.1";
const int C_OK = 1;
const int C_ERRO = 0;

// �����壺Buffer
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
* ARDrone �Զ��������ϸ����
*
*�����Ʋ�����
* ����ɣ���ɡ����䡢ǰ�������ˡ�����ɡ����ҷɡ������ٶ�
*����Ա����������
* ����ɣ���ȡ��ǰ��š���ȡǰ��š���ȡ����š�����ǰ���
*������ָ�
* ���ͻ���ָ����ͷ��п���ָ��
*����ʼ��������
* ���ͷ�������ʼ������ָ�socket��ʼ������
*������������
* ����ɣ����ݰ�������float����ת��int����
* ����ɣ���
*�����г�Ա������
* �������ݡ�ǰһ��ָ��
*��˽�г�Ա������
* ����ָ����׽��֣�һ�ף��������ٶȡ����������֡�����š�ǰ����š�������
*
* ԭ������Ϊ��������Ϊ
*/
class ARDrone
{
public:
	ARDrone(void){}
	ARDrone(char*);
	~ARDrone(void);

public:
	// ����������
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

	void setSpeed(int);		// ���÷����ٶ�

public:
	int		getCurrentSeq(){ return this->seq_; }			// get the current sequence
	int		getLastSeq(){ return this->lastSeq_; }		// get the last sequence
	int		getNextSeq();								// get the next sequence
	void	setLastSeq(int);							// set the last sequence

	int		send_at_cmd(char*);					// send all command
	int		send_pcmd(int, float, float, float, float);	// send control command
	void	parse(MemoryLibrary::Buffer&);				// ���ݰ�����
	void	initializeCmd();				// initialize command

protected:
	void	initializeSocketaddr();			// initialize sockaddr_in
	int		floatToInt(float);				// ʹ��������ʵ��float ת��int	

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