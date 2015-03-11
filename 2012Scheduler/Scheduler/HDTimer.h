// HDTimer.h: interface for the CHDTimer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HDTIMER_H__2C94A0DA_93BD_4BF7_90EB_5C8ACC8B5E3C__INCLUDED_)
#define AFX_HDTIMER_H__2C94A0DA_93BD_4BF7_90EB_5C8ACC8B5E3C__INCLUDED_

#include "Scheduler.h"
#include <windows.h>
#include <mmsystem.h>			//head file
#pragma comment(lib,"winmm")	//lib file
#define WM_MMTimer	WM_USER+100

#define FUNCNAMELEN	256

class CHDTimer  
{
public:
	void StopTimer();
	void SendTimerMsg();
	unsigned int StartTimer();
	unsigned int m_uiTimerID;
	bool CreateTimer(
		unsigned long msInterval=1,						//定时器间隔
		unsigned long ulEventType=TIME_PERIODIC,		//定时器类型
		unsigned long ulMsg = WM_MMTimer				//定时器返回的消息
	);

	CHDTimer();
	virtual ~CHDTimer();

	char strFunc[FUNCNAMELEN];
	void *pParameter;
	unsigned long m_ulInterval;		//定时器间隔
	int m_iLinkToCtrl;
private:
	unsigned long m_ulMsg;
	unsigned long m_ulEventType;	//定时器类型TIME_PERIODIC(0x0001),TIME_ONESHOT(0x0000)
};

#endif // !defined(AFX_HDTIMER_H__2C94A0DA_93BD_4BF7_90EB_5C8ACC8B5E3C__INCLUDED_)
