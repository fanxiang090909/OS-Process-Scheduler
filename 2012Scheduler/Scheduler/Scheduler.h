#ifndef __SCHEDULER_H_
#define __SCHEDULER_H_

#include	"HDTimer.h"

#define		MAX_PROCESSES	512

#define		TIME_SLICE_FIVE		5
#define		TIME_SLICE_TEN		10
#define		TIME_SLICE_FIFTEEN	15
#define		TIME_SLICE_TWENTY	20

typedef struct KPCB
{
	int	iProcessID;
	int iPeriod;	// �������ڣ����룩��-1Ϊ�����ڽ���
	int iPriority;	// ���ȼ���1-32��
}KPCB;

#include <stdio.h>

__declspec(dllexport) int Scheduler_CreateProcess(int iPriority, int iPeriod);
__declspec(dllexport) int Scheduler_SuspendProcess(int iProcessID);
__declspec(dllexport) int Scheduler_ResumeProcess(int iProcessID);
__declspec(dllexport) int Scheduler_StopProcess(int	iProcessID);
__declspec(dllexport) int Scheduler_StartProcess(int iProcessID);
__declspec(dllexport) int Scheduler();

#endif