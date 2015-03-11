
#include  "Scheduler.h"
#include "string.h"
CHDTimer HDTimer;

// 全部创建的进程
KPCB pcbList[MAX_PROCESSES];
// 记录对应pcbList的进程时间片剩余执行时间，单位ms, 如果是-1说明该处进程还未创建
int pcbListTimeCount[MAX_PROCESSES] = {-1};

// 全部创建进程数
int pcbListLength = 0;
// 记录最新插入的进程标号，增加插入速度
int currentPcbNo = -1;

// 正在运行的进程，初始没有
KPCB * pcbRunning = pcbList;

/////////////////////////////////////////////////////
// 进程三状态数据结构

// 正在运行的进程
int running = -1;

// 挂起和周期进程就绪态使用一个顺序表（数组实现）
int suspendPeriodList[MAX_PROCESSES] = {-1};

// 被挂起的进程顺序表
int * suspendList = &suspendPeriodList[MAX_PROCESSES - 1];
int suspendListLength = 0;

// 就绪态进程：一个周期进程顺序表、两个优先级队列
// 周期进程顺序表
int * periodList = &suspendPeriodList[0];
// 链表长度
int periodListLength = 0;

// 两个优先级队列，activatePQ与expiredPQ轮流指向
int PQA[MAX_PROCESSES] = {-1};
int PQB[MAX_PROCESSES] = {-1};
// 本轮未被执行的优先级队列，初始指向PQA
int * activatePQ = PQA;
// 堆长度
int activatePQLength = 0;
// 本轮刚执行完的优先级队列，初始指向PQB
int * expiredPQ = PQB;
// 堆长度
int expiredPQLength = 0;

////////////////////////////////////////////////////
// 定义互斥临界区域
CRITICAL_SECTION pcbListCS;
CRITICAL_SECTION suspendPeriodListCS;
CRITICAL_SECTION activeExpiredPQCS;

////////////////////////////////////////////////////
// 自定义函数声明
// 全部进程结构体数组操作
int pcb_insert(int period, int priority, int time_slice);
int pcb_delete(int processid);
// 计算时间片
int count_timeslice(int period, int priority);

// 对于顺序表的操作
int list_insert(int * start, int & length, int direction, int pos, int i);
int list_delete(int * start, int & length, int direction, int i);
int list_get(int * start, int & length, int direction, int i);
// 对优先级队列的操作
int PQ_insert(int * pq, int & length, int i);
int PQ_removemax(int * pq, int & length);
int PQ_removei(int * pq, int & length, int i);
void PQ_swap(int & i, int & j);
int PQ_compare(int i, int j);

// 已知不可修改函数声明
int RunProcess(int iProcessID);
int SuspendProcess(int iProcessID);

////////////////////////////////////////////////////
// 自定义函数实现

// 插入新的进程到pcbList中，返回processid号
int pcb_insert(int period, int priority, int time_slice)
{
	// 计算时间片
	time_slice = count_timeslice(period, priority);
	if (time_slice == -2) // period属性不符合要求
		return -2;
	else if (time_slice == -3) // priority属性不符合要求
		return -3;
	
	//////////////////////////////////
	InitializeCriticalSection (&pcbListCS); ///初始化临界区变量 

	// 计算插入位置
	if (++currentPcbNo >= 512)
	{
		int pos = 0;
		while (pcbListTimeCount[pos] >= 0)
			pos++;
		currentPcbNo = pos;
	}

	// 可以插入
	if (currentPcbNo < 512)
	{
		pcbList[currentPcbNo].iProcessID = currentPcbNo;
		pcbList[currentPcbNo].iPeriod = period;
		pcbList[currentPcbNo].iPriority = priority;
		pcbListTimeCount[currentPcbNo] = time_slice;
		pcbListLength++; // 长度增加

		//////////////////////////////////
		LeaveCriticalSection (&pcbListCS); ///撤销保护机制 
		return currentPcbNo;
	}
	else // 512个进程已满，不能插入 
		currentPcbNo--;

	//////////////////////////////////
	LeaveCriticalSection (&pcbListCS); ///撤销保护机制 
	
	return -1;
}

// 从pcbList中删除进程，即把标志位pcbListTimeCount置-1
int pcb_delete(int processid)
{
	if (processid < 0 || processid >= 512)
		return -1;	// 进程标号越界
	if (pcbListTimeCount[processid] == -1)
		return -2;  // 该处无进程

	//////////////////////////////////
	InitializeCriticalSection (&pcbListCS); ///初始化临界区变量 

	pcbListTimeCount[processid] = -1;
	pcbList[processid].iPeriod = 0;
	pcbList[processid].iPriority = 0;
	pcbList[processid].iProcessID = 0;
	pcbListLength--;

	//////////////////////////////////
	LeaveCriticalSection (&pcbListCS); ///撤销保护机制

	return 0; // 成功删除
}

// 时间片计算函数，在创建进程和时间片用完重新计时时使用
int count_timeslice(int period, int priority)
{
	int time_slice;
	// 新的进程属性不符合要求
	if (period < -1)
		return -2;
	// 检验属性priority并定义时间片
	if (priority > 0 && priority <= 8)
		time_slice = 5;
	else if (priority > 8 && priority <= 16)
		time_slice = 10;
	else if (priority > 16 && priority <= 24)
		time_slice = 15;
	else if (priority > 24 && priority <= 32)
		time_slice = 20;
	else
		return -3; // 优先级priority不符合要求

	if (period == 0) // 周期为0的周期进程？？？
		return -2; // 周期不符合要求
	else if (period > 0) // 周期进程要考虑时间片不大于iPeriod
	{	
		// 定义如果period / 3小于time_slice的话，将time_slice定义为period / 3
		if (period / 3 < time_slice && (period / 3) != 0)
			time_slice = period / 3;
		else if (period / 3 < time_slice && (period / 3) == 0)
			time_slice = 1;
	}
	return time_slice;
}

// direction:对于定义顺序表的正向0或逆向1，
//				比如suspendList和periodList共同使用suspendPeriodList数组空间
//					suspendList逆向1，periodList正向0
// 将元素i插入到顺序表pos位置之后
int list_insert(int * start, int & length, int direction, int pos, int i)
{
	if (pos >= length || list_get(start, length, direction, i) != -1)
		return -1;
	
	int step = 1; // 步长
	if (direction == 1)
	{	
		step = -1;
	
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
		for (int j = step * length; j < step * (pos + 1); j++)
			start[j] = start[j - step];
		start[step * (pos + 1)] = i;
		length++;
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
		return 0;
	}
	// 对于正向的插入验证是否为周期进程，若不是，返回
	if (pcbList[i].iPeriod == -1)
		return -2;
	
	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
	for (int j = step * length; j > step * (pos + 1); j--) 
		start[j] = start[j - step];
	start[step * (pos + 1)] = i;
	length++;
	//////////////////////////////////
	LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
	return 0;
}

// 将元素i在顺序表中删除
int list_delete(int * start, int & length, int direction, int i)
{
	int pos = list_get(start, length, direction, i);
	// 未找到该元素
	if (pos == -1)
		return -1;
	
	int step = 1;
	if (direction == 1)
	{	
		step = -1;
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
		for (int j = step * pos; j > step * length; j--)
			start[j] = start[j + step];
 		start[step * (--length)] = -1;
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
		return 0;
	}

	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
	for (int j = step * pos; j < step * length; j++)
		start[j] = start[j + step];
	start[step * (--length)] = -1;
	//////////////////////////////////
	LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
	return 0;
}

// 得到顺序表的中值为i的元素，返回其在顺序表中的位置
int list_get(int * start, int & length, int direction, int i)
{
	if (i < 0 || i >= MAX_PROCESSES)
		return -1; // 越界，不可能找到

	int step = 1; // 步长
	if (direction == 1)
	{	
		step = -1;
		int j = 0;
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
		while(j * step < length && start[j] != i && j > step * length)
		{
			j += step;
		}
		if (start[j] == i  && j * step < length)
		{	//////////////////////////////////
			LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
			return j * step; // 在顺序表中的位置
		}
		else
		{
			//////////////////////////////////
			LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
			return -1; // 没找到
		}
	}
	int j = 0;
	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
	while(j * step < length && j < length && start[j] != i && j < step * length)
	{
		j += step;
	}
	if (start[j] == i && j * step < length)
	{
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
		return j * step; // 在顺序表中的位置
	}
	else 
	{
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///释放临界区变量 
		return -1; // 没找到
	}
}

// 堆插入，把i插入到pq堆实现的优先级队列中
int PQ_insert(int * pq, int & length, int i)
{
	// 看pcbList中是否有此进程，没有则退出函数
	if (pcbListTimeCount[i] == -1)
		return -1;
	// 判断是否堆满
	if (length >= MAX_PROCESSES)
		return -2; // 堆满不能插入

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); ///初始化临界区变量 
	
	length++;
	pq[length - 1] = i;

	// 堆的向上调整
	int temppos = length - 1;
	while((temppos - 1) / 2 >= 0)
	{
		int compareresult = PQ_compare(pq[(temppos - 1) / 2], pq[temppos]);
		// 如果父节点的优先级小于当前结点
		if (compareresult == -1)
		{
			PQ_swap(pq[(temppos - 1) / 2], pq[temppos]);
			temppos = (temppos - 1) / 2;
		} 
		else if (compareresult >= 0) // 如果父节点的优先级大于或等于当前结点
		{
			break;
		}
		else // 无法比较，不添加插入堆
		{
			length--;
			pq[length - 1] = -1;
			break;
		}
	}
	//////////////////////////////////
	LeaveCriticalSection (&activeExpiredPQCS); /// 释放临界区变量
	return 0;
}

// 堆顶删除
int PQ_removemax(int * pq, int & length)
{
	// 已为空堆，不能再删除
	if (length == 0)
		return -1;

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); /// 初始化临界区变量 
	
	pq[0] = pq[length - 1];
	pq[--length] = -1; // 堆长度减1
	
	// 堆的向下调整函数
	int temppos = 0;
	// 左子树不为空
	while (temppos * 2 + 1 < length)
	{
		// 指向优先级最大子树根结点
		int priorityChild = temppos * 2 + 1;
		// 若左子树根节点优先级小于右子树，指向优先级最大子树根结点为右子树根节点
		if (temppos * 2 + 2 < length && PQ_compare(pq[temppos * 2 + 1], pq[temppos * 2 + 2]) == -1)
			priorityChild++;
		// 如果根节点优先级小于子结点
		if (PQ_compare(pq[temppos], pq[priorityChild]) == -1)
		{
			PQ_swap(pq[temppos], pq[priorityChild]);
			temppos = priorityChild;
		} else
			break;
	}
	//////////////////////////////////
	LeaveCriticalSection (&activeExpiredPQCS); /// 释放临界区变量
	return 0;
}

// 删除堆中的元素（在挂起进程suspendProcess中会用到），i是堆中的元素
int PQ_removei(int * pq, int & length, int i)
{
	// 已为空堆，不能再删除
	if (length == 0)
		return -1;

	// 由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
	int pos = list_get(pq, length, 0, i);
	if (pos == -1)
		return -2; // 未找到元素i，不能删除

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); ///初始化临界区变量 

	// 堆尾填补删除元素，堆长度减1
	pq[pos] = pq[length - 1];
	pq[--length] = -1; 

	// 最初调整位置
	int filterpos = (length - 2) / 2;
	while (filterpos >= 0)
	{
		// 堆的向下调整函数
		int temppos = filterpos;
		// 左子树不为空
		while (temppos * 2 + 1 < length)
		{
			// 指向优先级最大子树根结点
			int priorityChild = temppos * 2 + 1;
			// 若左子树根节点优先级小于右子树，指向优先级最大子树根结点为右子树根节点
			if (temppos * 2 + 2 < length && PQ_compare(pq[temppos * 2 + 1], pq[temppos * 2 + 2]) == -1)
				priorityChild++;
			// 如果根节点优先级小于子结点
			if (PQ_compare(pq[temppos], pq[priorityChild]) == -1)
			{
				PQ_swap(pq[temppos], pq[priorityChild]);
				temppos = priorityChild;
			} else
				break;
		}
	
		filterpos--;
	}

	//////////////////////////////////
	LeaveCriticalSection (&activeExpiredPQCS); ///释放临界区变量
	return 0;
}

// 优先级调整之结点交换函数
void PQ_swap(int & i, int & j)
{
	int temp = i;
	i = j;
	j = temp;
}

// 优先级比较函数：比较pcbList数组中下标i和j的进程优先级
// 返回值：-3进程不存在无法比较；-2不具有可比性；0相等；-1(i优先级小于j)；1(i优先级大于j)
int PQ_compare(int i, int j)
{
	// 进程不存在，无法比较
	if (pcbListTimeCount[i] == -1 || pcbListTimeCount[j] == -1)
		return -3;

	// 若其中之一为周期进程，不在这里比较，因为不具有可比性
	if (pcbList[i].iPeriod != -1 || pcbList[j].iPeriod != -1)
		return -2;
	else // 若均为非周期进程，只比较iPriority优先级
	{
		if (pcbList[i].iPriority < pcbList[j].iPriority)
			return -1; // i优先级小于j
		else if (pcbList[i].iPriority > pcbList[j].iPriority)
			return 1; // i优先级大于j
		else
			return 0; // i优先级等于j
	}
}

////////////////////////////////////////////////////

// Dllmain  初始化pcbList，id均为-1, 启动高精度时钟
BOOL WINAPI DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	//进程开始
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		printf("--> Scheduler started!\n");
		
		// 初始化各数据结构，赋各数组元素初值为-1
		memset(pcbListTimeCount, -1, sizeof(int) * MAX_PROCESSES);
		memset(suspendPeriodList, -1, sizeof(int) * MAX_PROCESSES);
		memset(PQA, -1, sizeof(int) * MAX_PROCESSES);
		memset(PQB, -1, sizeof(int) * MAX_PROCESSES);
		// 初始化临界区变量 
		InitializeCriticalSection (&pcbListCS); ///初始化临界区变量 
		InitializeCriticalSection (&suspendPeriodListCS); ///初始化临界区变量 
		InitializeCriticalSection (&activeExpiredPQCS); ///初始化临界区变量 

		HDTimer.StartTimer();
	}
	//进程结束
	if (fdwReason == DLL_PROCESS_DETACH)
	{
		HDTimer.StopTimer();
		printf("--> Scheduler exit!\n");
	}
	return TRUE;
}

// 创建进程，进入Suspend状态
// null -> suspend
int Scheduler_CreateProcess(int iPriority, int iPeriod)
{
	// 创建进程到pcbList
	int inPID = pcb_insert(iPeriod, iPriority, 0);
	if (inPID >= 0)
		printf("--> Process %d created!\n", inPID);
	else if (inPID == -1)
	{
		printf("--> Process(%d, %d) can't be created! No more space for the process!\n", iPriority, iPeriod);
		return -1;
	}
	else if (inPID == -2)
	{
		printf("--> Process(%d, %d) can't be created! Period value not allowed!\n", iPriority, iPeriod);
		return -1;
	}
	else if (inPID == -3)
	{
		printf("--> Process(%d, %d) can't be created! Priority value not allowed!\n", iPriority, iPeriod);
		return -1;
	}

	// 插入suspendList,数组尾插入
	list_insert(suspendList, suspendListLength, 1, suspendListLength - 1, inPID);

	// 数组头插入
	//list_insert(suspendList, suspendListLength, 1, -1, inPID);

	// 插入priorityList,数组尾插入
	//list_insert(periodList, periodListLength, 0, periodListLength - 1, inPID);

	return 0;
}

// 挂起进程，进程状态从其他的状态进入Suspend状态
// running -> suspend; ready -> suspend
// Suspend状态为阻塞状态，与SuspendProcess函数进入就绪态不同
int Scheduler_SuspendProcess(int iProcessID)
{
	// 删除状态，0为成功删除
	int result = -1;
	// 先在running中找
	if (running == iProcessID)
	{
		// 若找到，马上终止该进程
		running = -1;
		result = 0;
	}
	// 若running中不是，在periodList中从删除iProcessID，从周期进程就绪状态中删除
	else
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// 若在periodList中没有找到（即result < 0）
		// 然后从activatePQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// 若仍未找到
	if (result < 0)
		// 然后从expiredPQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// 还是查找不到可以挂起的该进程，要么已经在suspendList中，要么就是不符合条件
	if (result < 0) 
	{
		printf("--> Process %d can't be suspended! The process is not in ready/running state!\n", iProcessID);
		return -1; 
	}

	// 插入suspendList,数组尾插入
	list_insert(suspendList, suspendListLength, 1, suspendListLength - 1, iProcessID);
	printf("--> Process %d suspend!\n", iProcessID);
	
	return 0;
}

// 开始进程
// 【注意！】原给注释（进程进入ready态）有问题，进入ready态的应该是Scheduler_Resume调度
// 导致这个函数对应哪个状态转换不明确，把它按以下理解
// running -> ready 的同时 ready -> running
int Scheduler_StartProcess(int iProcessID)
{
	// 若正处在就绪态，不用重新开始
	if (running == iProcessID)
	{
		if (pcbListTimeCount[running] == 0)
			// 重新设置时间片
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
		return -1;
	}

	// 删除状态，0为成功删除
	int result = -1;
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// 若在periodList中没有找到（即result < 0）
		// 然后从activatePQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// 若仍未找到
	if (result < 0)
		// 然后从expiredPQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// 还是查找不到可以挂起的该进程，要么已经在suspendList中，要么就是不符合条件
	if (result < 0) 
	{
		printf("--> Process %d can't be started! The process is not ready!\n", iProcessID);
		return -2; // 已经在suspendList中都不能start
	}
	if (running != -1)
		// 若正在运行的进程已经执行完它的时间片，且该进程为普通进程
		if (pcbListTimeCount[running] == 0 && pcbList[running].iPeriod == -1)
		{	
			// 重新设置时间片
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
			// 插入expiredPQ优先级队列
			PQ_insert(expiredPQ, expiredPQLength, running);
			running = -1;
		}
		else // 若正在运行的进程未执行完它的时间片，且该进程为普通进程
		if (pcbListTimeCount[running] > 0 && pcbList[running].iPeriod == -1)
		{
			// 无需重设时间片，重新插入activatePQ优先级队列
			PQ_insert(activatePQ, activatePQLength, running);
			// 挂起进程，恢复到就绪态
			SuspendProcess(running);
			running = -1;
		}
		else // 若正在运行的进程为周期进程，且它已经执行完它的时间片 
		if (pcbListTimeCount[running] == 0 && pcbList[running].iPeriod >= 0)
		{
			// 重新设置时间片
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
			// 插入periodList顺序表，“数组尾”插入（这个很关键），相当于队列
			list_insert(periodList, periodListLength, 0, periodListLength - 1, running);
			running = -1;
		}
		else // 若正在运行的进程为周期进程，且它未执行完它的时间片 
		{
			// 插入periodList顺序表
			list_insert(periodList, periodListLength, 0, periodListLength - 1, running);
			// 挂起进程，恢复到就绪态
			SuspendProcess(running);
			running = -1;
		}

	// 正在运行的进程修改索引加指针
	running = iProcessID;
	pcbRunning = &pcbList[running];

	printf("--> Process %d started!\n", iProcessID);
	// 恢复进程即立即执行
	RunProcess(iProcessID);
	return 0;
}

// 杀死进程
// running -> null; ready -> null; suspend -> null
int Scheduler_StopProcess(int iProcessID)
{
	int result = -1;
	// 删除进程空间
	result = pcb_delete(iProcessID);
	if (result == -1) // 标号越界
	{
		printf("--> Process %d can't be stoped! The process is not exist!\n", iProcessID);
		return -1;
	}
	if (result == -2) // 结构体数组中该元素处并无进程
	{
		printf("--> Process %d can't be stoped! The process is not exist!\n", iProcessID);
		return -2;
	}

	// 删除三状态中的进程索引
	result = -1;
	// 先在running中找
	if (running == iProcessID)
	{
		// 若找到，马上终止该进程
		running = -1;
		result = 0;
	}
	// 若running中不是，在periodList中，从周期进程就绪状态中删除
	else
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// 若在periodList中没有找到（即result < 0）
		// 然后从activatePQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// 若仍未找到
	if (result < 0)
		// 然后从expiredPQ中找，由于该堆是数组实现，可用list_get查找正向顺序表元素i的位置
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// 还是查找不到可以挂起的该进程，要么已经在suspendList中，要么就是不符合条件
	if (result < 0) 
		result = list_delete(suspendList, suspendListLength, 1, iProcessID);
	// 状态中没有可以删除的
	if (result < 0)
		return -3;

	printf("--> Process %d stoped!\n", iProcessID);
	return 0;
}

// 重新开始进程
// suspend -> ready
int Scheduler_ResumeProcess(int iProcessID)
{
	// 进程号为iProcessID的进程不存在
	if (pcbListTimeCount[iProcessID] == -1 || iProcessID < 0 || iProcessID >= 512)
	{
		printf("--> Process %d can't be resumed! The process is not exist!\n", iProcessID);
		return -1;
	}

	int result = -1;
	// 从挂起进程状态中删除
	result = list_delete(suspendList, suspendListLength, 1, iProcessID);
		
	if (result == 0)
	{
		// 插入就绪态,判断是否为非周期进程
		// 若是周期进程，插入priorityList,数组尾插入
		if (pcbList[iProcessID].iPeriod != -1)
			list_insert(periodList, periodListLength, 0, periodListLength - 1, iProcessID);
		else // 普通优先级队列，插入优先级activatePQ队列
			PQ_insert(activatePQ, activatePQLength, iProcessID);

		printf("--> Process %d resumed!\n", iProcessID);
	}
	else
		printf("--> Process %d can't be resumed! The process is not in suspended-list.\n", iProcessID);
	return 0;
}

// 进程退出，不引发调度
int Scheduler_ProcessExit(int iProcessID)
{
	printf("--> Process %d exit!\n", iProcessID);
	return 0;
}

/////////////////////////////////////////////
// 运行进程，不维护pcbList
// 本函数不得更改
int RunProcess(int iProcessID)
{
	printf("--> Do Process %d started!\n", iProcessID);
	return 0;
}

// 挂起进程，不维护pcbList
//
// 【注意！】此函数SuspendProcess并不是严格意义上的进入阻塞态
// 而是进程调度中引发Scheduler_Start时，正在执行的进程返回进入就绪态
//
// 本函数不得更改
int SuspendProcess(int iProcessID)
{
	printf("--> Do Process %d suspend!\n", iProcessID);
	return 0;
}


// 每一毫秒执行一次，调度运行或"挂起"进程。
// 【注意！】上行注释中这个Scheduler中不应该挂起进程，顶多是running -> ready
int Scheduler()
{
	if (running != -1)
		printf("--> Begin schedule, Process %d is running now!\n", pcbRunning->iProcessID);
	else 
		printf("--> Begin schedule, No process is running!\n");
	///////////////此处加入调度算法//////////////////////

	// 如果有进程正在运行，记录剩余时间
	if (running != -1 && pcbListTimeCount[running] > 0)
		pcbListTimeCount[running]--;
	
	int pid = -1;
	// 对所有就绪状态中的周期进程，记录时间片剩余时间
	for (int i = 0; i < periodListLength; i++)
	{	
		pid =  periodList[i];
		// 调度出现问题，返回-1
		if (pid == -1 || pcbListTimeCount[pid] == -1)
		{	
			printf("-->Error: in period process!\n");
			continue;
		}
		// running中的已经在前面记录过减一后的值
		if (pcbListTimeCount[pid] != 0 && pid != running)
			pcbListTimeCount[pid]--; // 记录时间片剩余时间
	}

	// 标识是否已经执行调度，0表示还未执行
	int isScheduler = 0;

	// 最先执行周期进程
	for (int i = 0; i < periodListLength; i++)
	{	
		pid =  periodList[i];
		// 调度出现问题，返回-1
		if (pid == -1 || pcbListTimeCount[pid] == -1)
		{	
			printf("-->Error: in period process!\n");
			continue;
		}

		// 有周期进程到达周期期限或没有进程在运行
		if (isScheduler == 0 && 
			// 若正在运行的进程为周期进程，且它已完成时间片，即pcbListTimeCount[running] == 0
			((running != -1 && pcbListTimeCount[pid] == 0 && pcbList[running].iPeriod >= 0 && pcbListTimeCount[running] == 0)
			// 若正在运行的进程为非周期进程，可抢占
			|| (running != -1 && pcbListTimeCount[pid] == 0 && pcbList[running].iPeriod == -1)
			|| running == -1
			))
		{
			// 有则抢占运行
			//
			// 【注意!】
			// 所有的RunProcess和SuspendProcess函数均在Scheduler_ResumeProcess函数中处理
			Scheduler_StartProcess(pid);
			isScheduler = 1;
		}
	}

	// 周期进程处理结束后，处理普通优先级进程
	pid = -1;
	// 如果没有引发Scheduler_Start函数，继续调度普通优先级进程就绪态中的最大优先级进程
	if (isScheduler == 0)
	{
		// 若普通优先级进程中还有待运行的进程
		if (activatePQLength > 0)
			pid = activatePQ[0];
		// 若普通优先级进程中没有待运行的进程（正在运行的进程时间片已执行完），但是有已执行完的进程
		if (pid == -1 && expiredPQLength != 0 && pcbListTimeCount[running] == 0)
		{
			// 交换activatePQ和expiredPQ指针及长度值
		
			//////////////////////////////////
			InitializeCriticalSection (&activeExpiredPQCS); ///初始化临界区变量 
			
			int * temp = activatePQ;
			activatePQ = expiredPQ;
			expiredPQ = temp;
			int tempLength = activatePQLength;
			activatePQLength = expiredPQLength;
			expiredPQLength = tempLength;

			//////////////////////////////////
			LeaveCriticalSection (&activeExpiredPQCS); ///释放临界区变量
		
			if (activatePQLength > 0)
				pid = activatePQ[0]; 
		}
		// 如果activate中有优先级高的进程且或者没有正在运行的进程
						// 若正在运行的进程是非周期进程，且有优先级更高的进程，可抢占
		if (pid != -1 && ((running != -1 && pcbList[running].iPeriod == -1 && PQ_compare(running, pid) == -1)
						// 若正在运行的进程已完成时间片			
						|| (running != -1 && pcbListTimeCount[running] == 0) 
						|| running == -1))
		{	
			// 有则抢占运行
			//
			// 【注意!】
			// 所有的RunProcess和SuspendProcess函数均在Scheduler_StartProcess函数中处理
			Scheduler_StartProcess(pid);
			isScheduler = 1;
		}
	}

	// 若还未引发调度
	if (isScheduler == 0 && running != -1 && pcbListTimeCount[running] == 0)
		Scheduler_StartProcess(running);
	
	///////////////调度算法结束//////////////////////////
	if (running != -1)
		printf("--> End schedule, Process %d is running!\n", pcbRunning->iProcessID);
	else 
		printf("--> End schedule, No process is running!\n");
	return 0;
}
//////////////////////////////////////////////////
