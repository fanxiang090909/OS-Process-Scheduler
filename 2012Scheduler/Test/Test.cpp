
#include "..\Scheduler\Scheduler.h"
#include <cstdio>

#ifndef CMDTYPE
#define CMDTYPE

#define CREATE 0
#define RESUME 1
#define START 2
#define STOP 3
#define EXIT 4
#define SUSPEND 5

#endif

void main(int argc, char * argv[])
{
	FILE * file;

	char * filename;
	char command[10] = {'\0'};
	int priority = -1;
	int period = -2;
	filename = argv[1];

	if (filename == NULL || (file = fopen(filename, "r")) == NULL)
	{
		printf("不能打开测试文件%s\n", filename);
		exit(1);
		return;
	}

	while (!feof(file))
	{
		fscanf(file, "%s\t%d\t%d", command, &priority, &period);
		printf("%s %d %d\n", command, priority, period);
		if (command == NULL || command[0] == EOF)
		{
			fscanf(file, "%s\t%d\t%d", command, &priority, &period);
			continue;
		}

		// 命令类型
		int cmdtype = 0;
		if (strcmp(command, "create") == 0)
			cmdtype = CREATE;
		else if (strcmp(command, "resume") == 0)
			cmdtype = RESUME;
		else if (strcmp(command, "suspend") == 0)
			cmdtype = SUSPEND;
		else if (strcmp(command, "start") == 0)
			cmdtype = START;
		else if (strcmp(command, "stop") == 0)
			cmdtype = STOP;
		else if (strcmp(command, "exit") == 0)
			cmdtype = EXIT;
		else // 这条命令错误
		{
			// 输出错误命令
			printf("The input command \"%s %d %d\" is illegal.\n", command, priority, period);
			fscanf(file, "%s\t%d\t%d", command, &priority, &period);
			continue;
		}

		switch (cmdtype)
		{
		case 0:
			Scheduler_CreateProcess(priority, period);
			break;
		case 1:
			Scheduler_ResumeProcess(priority);
			break;
		case 2:
			Scheduler_StartProcess(priority);
			break;
		case 3:
			Scheduler_StopProcess(priority);
			break;
		case 4:
			// Scheduler_ExitProcess(priority);
			break;
		case 5:
			Scheduler_SuspendProcess(priority);
			break;
		default:
			;
		}

		priority = -1;
		period = -2;
	}

	fclose(file);

	// 以后完全开始自由调度，以上执行中自由调度和外部调度可能互相穿插

	/*	
	Scheduler_CreateProcess(1, -1);//0
	Scheduler_CreateProcess(2, -1);//1
	Scheduler_CreateProcess(3, -1);//2
	Scheduler_ResumeProcess(1);
	Scheduler_CreateProcess(4, -1);//3
	Scheduler_ResumeProcess(2);
	Scheduler_CreateProcess(5, -1);//4
	Scheduler_CreateProcess(6, -1);//5
	Scheduler_CreateProcess(7, -1);//6
	Scheduler_CreateProcess(8, -1);//7
	Scheduler_CreateProcess(9, -1);
	Scheduler_CreateProcess(10, -1);
	Scheduler_CreateProcess(11, -1);
	Scheduler_CreateProcess(12, -1);
	Scheduler_CreateProcess(13, -1);
	Scheduler_CreateProcess(14, -1);
	Scheduler_CreateProcess(15, -1);
	Scheduler_CreateProcess(16, -1);//15
	Scheduler_ResumeProcess(3);
	Scheduler_ResumeProcess(4);
	Scheduler_ResumeProcess(5);
	Scheduler_ResumeProcess(6);
	Scheduler_ResumeProcess(7);
	Scheduler_ResumeProcess(8);
	Scheduler_ResumeProcess(9);
	Scheduler_ResumeProcess(10);
	Scheduler_ResumeProcess(11);
	Scheduler_ResumeProcess(12);
	Scheduler_ResumeProcess(13);
	Scheduler_ResumeProcess(14);
	Scheduler_ResumeProcess(15);
	Scheduler_CreateProcess(17, 8);//16
	Scheduler_CreateProcess(17, 8);//17
	Scheduler_ResumeProcess(16);
	*/
	
	while(1)
	{
		Sleep(100);
	}
}