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
	int iPeriod;	// 进程周期（毫秒），-1为非周期进程
	int iPriority;	// 优先级。1-32级
}KPCB;

#include <stdio.h>

__declspec(dllexport) int Scheduler_CreateProcess(int iPriority, int iPeriod);
__declspec(dllexport) int Scheduler_SuspendProcess(int iProcessID);
__declspec(dllexport) int Scheduler_ResumeProcess(int iProcessID);
__declspec(dllexport) int Scheduler_StopProcess(int	iProcessID);
__declspec(dllexport) int Scheduler_StartProcess(int iProcessID);
__declspec(dllexport) int Scheduler();

#endif