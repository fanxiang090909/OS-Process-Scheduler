
#include  "Scheduler.h"
#include "string.h"
CHDTimer HDTimer;

// ȫ�������Ľ���
KPCB pcbList[MAX_PROCESSES];
// ��¼��ӦpcbList�Ľ���ʱ��Ƭʣ��ִ��ʱ�䣬��λms, �����-1˵���ô����̻�δ����
int pcbListTimeCount[MAX_PROCESSES] = {-1};

// ȫ������������
int pcbListLength = 0;
// ��¼���²���Ľ��̱�ţ����Ӳ����ٶ�
int currentPcbNo = -1;

// �������еĽ��̣���ʼû��
KPCB * pcbRunning = pcbList;

/////////////////////////////////////////////////////
// ������״̬���ݽṹ

// �������еĽ���
int running = -1;

// ��������ڽ��̾���̬ʹ��һ��˳�������ʵ�֣�
int suspendPeriodList[MAX_PROCESSES] = {-1};

// ������Ľ���˳���
int * suspendList = &suspendPeriodList[MAX_PROCESSES - 1];
int suspendListLength = 0;

// ����̬���̣�һ�����ڽ���˳����������ȼ�����
// ���ڽ���˳���
int * periodList = &suspendPeriodList[0];
// ������
int periodListLength = 0;

// �������ȼ����У�activatePQ��expiredPQ����ָ��
int PQA[MAX_PROCESSES] = {-1};
int PQB[MAX_PROCESSES] = {-1};
// ����δ��ִ�е����ȼ����У���ʼָ��PQA
int * activatePQ = PQA;
// �ѳ���
int activatePQLength = 0;
// ���ָ�ִ��������ȼ����У���ʼָ��PQB
int * expiredPQ = PQB;
// �ѳ���
int expiredPQLength = 0;

////////////////////////////////////////////////////
// ���廥���ٽ�����
CRITICAL_SECTION pcbListCS;
CRITICAL_SECTION suspendPeriodListCS;
CRITICAL_SECTION activeExpiredPQCS;

////////////////////////////////////////////////////
// �Զ��庯������
// ȫ�����̽ṹ���������
int pcb_insert(int period, int priority, int time_slice);
int pcb_delete(int processid);
// ����ʱ��Ƭ
int count_timeslice(int period, int priority);

// ����˳���Ĳ���
int list_insert(int * start, int & length, int direction, int pos, int i);
int list_delete(int * start, int & length, int direction, int i);
int list_get(int * start, int & length, int direction, int i);
// �����ȼ����еĲ���
int PQ_insert(int * pq, int & length, int i);
int PQ_removemax(int * pq, int & length);
int PQ_removei(int * pq, int & length, int i);
void PQ_swap(int & i, int & j);
int PQ_compare(int i, int j);

// ��֪�����޸ĺ�������
int RunProcess(int iProcessID);
int SuspendProcess(int iProcessID);

////////////////////////////////////////////////////
// �Զ��庯��ʵ��

// �����µĽ��̵�pcbList�У�����processid��
int pcb_insert(int period, int priority, int time_slice)
{
	// ����ʱ��Ƭ
	time_slice = count_timeslice(period, priority);
	if (time_slice == -2) // period���Բ�����Ҫ��
		return -2;
	else if (time_slice == -3) // priority���Բ�����Ҫ��
		return -3;
	
	//////////////////////////////////
	InitializeCriticalSection (&pcbListCS); ///��ʼ���ٽ������� 

	// �������λ��
	if (++currentPcbNo >= 512)
	{
		int pos = 0;
		while (pcbListTimeCount[pos] >= 0)
			pos++;
		currentPcbNo = pos;
	}

	// ���Բ���
	if (currentPcbNo < 512)
	{
		pcbList[currentPcbNo].iProcessID = currentPcbNo;
		pcbList[currentPcbNo].iPeriod = period;
		pcbList[currentPcbNo].iPriority = priority;
		pcbListTimeCount[currentPcbNo] = time_slice;
		pcbListLength++; // ��������

		//////////////////////////////////
		LeaveCriticalSection (&pcbListCS); ///������������ 
		return currentPcbNo;
	}
	else // 512���������������ܲ��� 
		currentPcbNo--;

	//////////////////////////////////
	LeaveCriticalSection (&pcbListCS); ///������������ 
	
	return -1;
}

// ��pcbList��ɾ�����̣����ѱ�־λpcbListTimeCount��-1
int pcb_delete(int processid)
{
	if (processid < 0 || processid >= 512)
		return -1;	// ���̱��Խ��
	if (pcbListTimeCount[processid] == -1)
		return -2;  // �ô��޽���

	//////////////////////////////////
	InitializeCriticalSection (&pcbListCS); ///��ʼ���ٽ������� 

	pcbListTimeCount[processid] = -1;
	pcbList[processid].iPeriod = 0;
	pcbList[processid].iPriority = 0;
	pcbList[processid].iProcessID = 0;
	pcbListLength--;

	//////////////////////////////////
	LeaveCriticalSection (&pcbListCS); ///������������

	return 0; // �ɹ�ɾ��
}

// ʱ��Ƭ���㺯�����ڴ������̺�ʱ��Ƭ�������¼�ʱʱʹ��
int count_timeslice(int period, int priority)
{
	int time_slice;
	// �µĽ������Բ�����Ҫ��
	if (period < -1)
		return -2;
	// ��������priority������ʱ��Ƭ
	if (priority > 0 && priority <= 8)
		time_slice = 5;
	else if (priority > 8 && priority <= 16)
		time_slice = 10;
	else if (priority > 16 && priority <= 24)
		time_slice = 15;
	else if (priority > 24 && priority <= 32)
		time_slice = 20;
	else
		return -3; // ���ȼ�priority������Ҫ��

	if (period == 0) // ����Ϊ0�����ڽ��̣�����
		return -2; // ���ڲ�����Ҫ��
	else if (period > 0) // ���ڽ���Ҫ����ʱ��Ƭ������iPeriod
	{	
		// �������period / 3С��time_slice�Ļ�����time_slice����Ϊperiod / 3
		if (period / 3 < time_slice && (period / 3) != 0)
			time_slice = period / 3;
		else if (period / 3 < time_slice && (period / 3) == 0)
			time_slice = 1;
	}
	return time_slice;
}

// direction:���ڶ���˳��������0������1��
//				����suspendList��periodList��ͬʹ��suspendPeriodList����ռ�
//					suspendList����1��periodList����0
// ��Ԫ��i���뵽˳���posλ��֮��
int list_insert(int * start, int & length, int direction, int pos, int i)
{
	if (pos >= length || list_get(start, length, direction, i) != -1)
		return -1;
	
	int step = 1; // ����
	if (direction == 1)
	{	
		step = -1;
	
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
		for (int j = step * length; j < step * (pos + 1); j++)
			start[j] = start[j - step];
		start[step * (pos + 1)] = i;
		length++;
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
		return 0;
	}
	// ��������Ĳ�����֤�Ƿ�Ϊ���ڽ��̣������ǣ�����
	if (pcbList[i].iPeriod == -1)
		return -2;
	
	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
	for (int j = step * length; j > step * (pos + 1); j--) 
		start[j] = start[j - step];
	start[step * (pos + 1)] = i;
	length++;
	//////////////////////////////////
	LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
	return 0;
}

// ��Ԫ��i��˳�����ɾ��
int list_delete(int * start, int & length, int direction, int i)
{
	int pos = list_get(start, length, direction, i);
	// δ�ҵ���Ԫ��
	if (pos == -1)
		return -1;
	
	int step = 1;
	if (direction == 1)
	{	
		step = -1;
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
		for (int j = step * pos; j > step * length; j--)
			start[j] = start[j + step];
 		start[step * (--length)] = -1;
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
		return 0;
	}

	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
	for (int j = step * pos; j < step * length; j++)
		start[j] = start[j + step];
	start[step * (--length)] = -1;
	//////////////////////////////////
	LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
	return 0;
}

// �õ�˳������ֵΪi��Ԫ�أ���������˳����е�λ��
int list_get(int * start, int & length, int direction, int i)
{
	if (i < 0 || i >= MAX_PROCESSES)
		return -1; // Խ�磬�������ҵ�

	int step = 1; // ����
	if (direction == 1)
	{	
		step = -1;
		int j = 0;
		//////////////////////////////////
		InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
		while(j * step < length && start[j] != i && j > step * length)
		{
			j += step;
		}
		if (start[j] == i  && j * step < length)
		{	//////////////////////////////////
			LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
			return j * step; // ��˳����е�λ��
		}
		else
		{
			//////////////////////////////////
			LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
			return -1; // û�ҵ�
		}
	}
	int j = 0;
	//////////////////////////////////
	InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
	while(j * step < length && j < length && start[j] != i && j < step * length)
	{
		j += step;
	}
	if (start[j] == i && j * step < length)
	{
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
		return j * step; // ��˳����е�λ��
	}
	else 
	{
		//////////////////////////////////
		LeaveCriticalSection (&suspendPeriodListCS); ///�ͷ��ٽ������� 
		return -1; // û�ҵ�
	}
}

// �Ѳ��룬��i���뵽pq��ʵ�ֵ����ȼ�������
int PQ_insert(int * pq, int & length, int i)
{
	// ��pcbList���Ƿ��д˽��̣�û�����˳�����
	if (pcbListTimeCount[i] == -1)
		return -1;
	// �ж��Ƿ����
	if (length >= MAX_PROCESSES)
		return -2; // �������ܲ���

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); ///��ʼ���ٽ������� 
	
	length++;
	pq[length - 1] = i;

	// �ѵ����ϵ���
	int temppos = length - 1;
	while((temppos - 1) / 2 >= 0)
	{
		int compareresult = PQ_compare(pq[(temppos - 1) / 2], pq[temppos]);
		// ������ڵ�����ȼ�С�ڵ�ǰ���
		if (compareresult == -1)
		{
			PQ_swap(pq[(temppos - 1) / 2], pq[temppos]);
			temppos = (temppos - 1) / 2;
		} 
		else if (compareresult >= 0) // ������ڵ�����ȼ����ڻ���ڵ�ǰ���
		{
			break;
		}
		else // �޷��Ƚϣ�����Ӳ����
		{
			length--;
			pq[length - 1] = -1;
			break;
		}
	}
	//////////////////////////////////
	LeaveCriticalSection (&activeExpiredPQCS); /// �ͷ��ٽ�������
	return 0;
}

// �Ѷ�ɾ��
int PQ_removemax(int * pq, int & length)
{
	// ��Ϊ�նѣ�������ɾ��
	if (length == 0)
		return -1;

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); /// ��ʼ���ٽ������� 
	
	pq[0] = pq[length - 1];
	pq[--length] = -1; // �ѳ��ȼ�1
	
	// �ѵ����µ�������
	int temppos = 0;
	// ��������Ϊ��
	while (temppos * 2 + 1 < length)
	{
		// ָ�����ȼ�������������
		int priorityChild = temppos * 2 + 1;
		// �����������ڵ����ȼ�С����������ָ�����ȼ�������������Ϊ���������ڵ�
		if (temppos * 2 + 2 < length && PQ_compare(pq[temppos * 2 + 1], pq[temppos * 2 + 2]) == -1)
			priorityChild++;
		// ������ڵ����ȼ�С���ӽ��
		if (PQ_compare(pq[temppos], pq[priorityChild]) == -1)
		{
			PQ_swap(pq[temppos], pq[priorityChild]);
			temppos = priorityChild;
		} else
			break;
	}
	//////////////////////////////////
	LeaveCriticalSection (&activeExpiredPQCS); /// �ͷ��ٽ�������
	return 0;
}

// ɾ�����е�Ԫ�أ��ڹ������suspendProcess�л��õ�����i�Ƕ��е�Ԫ��
int PQ_removei(int * pq, int & length, int i)
{
	// ��Ϊ�նѣ�������ɾ��
	if (length == 0)
		return -1;

	// ���ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
	int pos = list_get(pq, length, 0, i);
	if (pos == -1)
		return -2; // δ�ҵ�Ԫ��i������ɾ��

	//////////////////////////////////
	InitializeCriticalSection (&activeExpiredPQCS); ///��ʼ���ٽ������� 

	// ��β�ɾ��Ԫ�أ��ѳ��ȼ�1
	pq[pos] = pq[length - 1];
	pq[--length] = -1; 

	// �������λ��
	int filterpos = (length - 2) / 2;
	while (filterpos >= 0)
	{
		// �ѵ����µ�������
		int temppos = filterpos;
		// ��������Ϊ��
		while (temppos * 2 + 1 < length)
		{
			// ָ�����ȼ�������������
			int priorityChild = temppos * 2 + 1;
			// �����������ڵ����ȼ�С����������ָ�����ȼ�������������Ϊ���������ڵ�
			if (temppos * 2 + 2 < length && PQ_compare(pq[temppos * 2 + 1], pq[temppos * 2 + 2]) == -1)
				priorityChild++;
			// ������ڵ����ȼ�С���ӽ��
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
	LeaveCriticalSection (&activeExpiredPQCS); ///�ͷ��ٽ�������
	return 0;
}

// ���ȼ�����֮��㽻������
void PQ_swap(int & i, int & j)
{
	int temp = i;
	i = j;
	j = temp;
}

// ���ȼ��ȽϺ������Ƚ�pcbList�������±�i��j�Ľ������ȼ�
// ����ֵ��-3���̲������޷��Ƚϣ�-2�����пɱ��ԣ�0��ȣ�-1(i���ȼ�С��j)��1(i���ȼ�����j)
int PQ_compare(int i, int j)
{
	// ���̲����ڣ��޷��Ƚ�
	if (pcbListTimeCount[i] == -1 || pcbListTimeCount[j] == -1)
		return -3;

	// ������֮һΪ���ڽ��̣���������Ƚϣ���Ϊ�����пɱ���
	if (pcbList[i].iPeriod != -1 || pcbList[j].iPeriod != -1)
		return -2;
	else // ����Ϊ�����ڽ��̣�ֻ�Ƚ�iPriority���ȼ�
	{
		if (pcbList[i].iPriority < pcbList[j].iPriority)
			return -1; // i���ȼ�С��j
		else if (pcbList[i].iPriority > pcbList[j].iPriority)
			return 1; // i���ȼ�����j
		else
			return 0; // i���ȼ�����j
	}
}

////////////////////////////////////////////////////

// Dllmain  ��ʼ��pcbList��id��Ϊ-1, �����߾���ʱ��
BOOL WINAPI DllMain(HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	//���̿�ʼ
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		printf("--> Scheduler started!\n");
		
		// ��ʼ�������ݽṹ����������Ԫ�س�ֵΪ-1
		memset(pcbListTimeCount, -1, sizeof(int) * MAX_PROCESSES);
		memset(suspendPeriodList, -1, sizeof(int) * MAX_PROCESSES);
		memset(PQA, -1, sizeof(int) * MAX_PROCESSES);
		memset(PQB, -1, sizeof(int) * MAX_PROCESSES);
		// ��ʼ���ٽ������� 
		InitializeCriticalSection (&pcbListCS); ///��ʼ���ٽ������� 
		InitializeCriticalSection (&suspendPeriodListCS); ///��ʼ���ٽ������� 
		InitializeCriticalSection (&activeExpiredPQCS); ///��ʼ���ٽ������� 

		HDTimer.StartTimer();
	}
	//���̽���
	if (fdwReason == DLL_PROCESS_DETACH)
	{
		HDTimer.StopTimer();
		printf("--> Scheduler exit!\n");
	}
	return TRUE;
}

// �������̣�����Suspend״̬
// null -> suspend
int Scheduler_CreateProcess(int iPriority, int iPeriod)
{
	// �������̵�pcbList
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

	// ����suspendList,����β����
	list_insert(suspendList, suspendListLength, 1, suspendListLength - 1, inPID);

	// ����ͷ����
	//list_insert(suspendList, suspendListLength, 1, -1, inPID);

	// ����priorityList,����β����
	//list_insert(periodList, periodListLength, 0, periodListLength - 1, inPID);

	return 0;
}

// ������̣�����״̬��������״̬����Suspend״̬
// running -> suspend; ready -> suspend
// Suspend״̬Ϊ����״̬����SuspendProcess�����������̬��ͬ
int Scheduler_SuspendProcess(int iProcessID)
{
	// ɾ��״̬��0Ϊ�ɹ�ɾ��
	int result = -1;
	// ����running����
	if (running == iProcessID)
	{
		// ���ҵ���������ֹ�ý���
		running = -1;
		result = 0;
	}
	// ��running�в��ǣ���periodList�д�ɾ��iProcessID�������ڽ��̾���״̬��ɾ��
	else
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// ����periodList��û���ҵ�����result < 0��
		// Ȼ���activatePQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// ����δ�ҵ�
	if (result < 0)
		// Ȼ���expiredPQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// ���ǲ��Ҳ������Թ���ĸý��̣�Ҫô�Ѿ���suspendList�У�Ҫô���ǲ���������
	if (result < 0) 
	{
		printf("--> Process %d can't be suspended! The process is not in ready/running state!\n", iProcessID);
		return -1; 
	}

	// ����suspendList,����β����
	list_insert(suspendList, suspendListLength, 1, suspendListLength - 1, iProcessID);
	printf("--> Process %d suspend!\n", iProcessID);
	
	return 0;
}

// ��ʼ����
// ��ע�⣡��ԭ��ע�ͣ����̽���ready̬�������⣬����ready̬��Ӧ����Scheduler_Resume����
// �������������Ӧ�ĸ�״̬ת������ȷ���������������
// running -> ready ��ͬʱ ready -> running
int Scheduler_StartProcess(int iProcessID)
{
	// �������ھ���̬���������¿�ʼ
	if (running == iProcessID)
	{
		if (pcbListTimeCount[running] == 0)
			// ��������ʱ��Ƭ
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
		return -1;
	}

	// ɾ��״̬��0Ϊ�ɹ�ɾ��
	int result = -1;
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// ����periodList��û���ҵ�����result < 0��
		// Ȼ���activatePQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// ����δ�ҵ�
	if (result < 0)
		// Ȼ���expiredPQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// ���ǲ��Ҳ������Թ���ĸý��̣�Ҫô�Ѿ���suspendList�У�Ҫô���ǲ���������
	if (result < 0) 
	{
		printf("--> Process %d can't be started! The process is not ready!\n", iProcessID);
		return -2; // �Ѿ���suspendList�ж�����start
	}
	if (running != -1)
		// ���������еĽ����Ѿ�ִ��������ʱ��Ƭ���Ҹý���Ϊ��ͨ����
		if (pcbListTimeCount[running] == 0 && pcbList[running].iPeriod == -1)
		{	
			// ��������ʱ��Ƭ
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
			// ����expiredPQ���ȼ�����
			PQ_insert(expiredPQ, expiredPQLength, running);
			running = -1;
		}
		else // ���������еĽ���δִ��������ʱ��Ƭ���Ҹý���Ϊ��ͨ����
		if (pcbListTimeCount[running] > 0 && pcbList[running].iPeriod == -1)
		{
			// ��������ʱ��Ƭ�����²���activatePQ���ȼ�����
			PQ_insert(activatePQ, activatePQLength, running);
			// ������̣��ָ�������̬
			SuspendProcess(running);
			running = -1;
		}
		else // ���������еĽ���Ϊ���ڽ��̣������Ѿ�ִ��������ʱ��Ƭ 
		if (pcbListTimeCount[running] == 0 && pcbList[running].iPeriod >= 0)
		{
			// ��������ʱ��Ƭ
			pcbListTimeCount[running] = count_timeslice(pcbList[running].iPeriod, pcbList[running].iPriority);
			// ����periodList˳���������β�����루����ܹؼ������൱�ڶ���
			list_insert(periodList, periodListLength, 0, periodListLength - 1, running);
			running = -1;
		}
		else // ���������еĽ���Ϊ���ڽ��̣�����δִ��������ʱ��Ƭ 
		{
			// ����periodList˳���
			list_insert(periodList, periodListLength, 0, periodListLength - 1, running);
			// ������̣��ָ�������̬
			SuspendProcess(running);
			running = -1;
		}

	// �������еĽ����޸�������ָ��
	running = iProcessID;
	pcbRunning = &pcbList[running];

	printf("--> Process %d started!\n", iProcessID);
	// �ָ����̼�����ִ��
	RunProcess(iProcessID);
	return 0;
}

// ɱ������
// running -> null; ready -> null; suspend -> null
int Scheduler_StopProcess(int iProcessID)
{
	int result = -1;
	// ɾ�����̿ռ�
	result = pcb_delete(iProcessID);
	if (result == -1) // ���Խ��
	{
		printf("--> Process %d can't be stoped! The process is not exist!\n", iProcessID);
		return -1;
	}
	if (result == -2) // �ṹ�������и�Ԫ�ش����޽���
	{
		printf("--> Process %d can't be stoped! The process is not exist!\n", iProcessID);
		return -2;
	}

	// ɾ����״̬�еĽ�������
	result = -1;
	// ����running����
	if (running == iProcessID)
	{
		// ���ҵ���������ֹ�ý���
		running = -1;
		result = 0;
	}
	// ��running�в��ǣ���periodList�У������ڽ��̾���״̬��ɾ��
	else
		result = list_delete(periodList, periodListLength, 0, iProcessID);
	if (result < 0)
		// ����periodList��û���ҵ�����result < 0��
		// Ȼ���activatePQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(activatePQ, activatePQLength, iProcessID);
	// ����δ�ҵ�
	if (result < 0)
		// Ȼ���expiredPQ���ң����ڸö�������ʵ�֣�����list_get��������˳���Ԫ��i��λ��
		result = PQ_removei(expiredPQ, expiredPQLength, iProcessID);
	// ���ǲ��Ҳ������Թ���ĸý��̣�Ҫô�Ѿ���suspendList�У�Ҫô���ǲ���������
	if (result < 0) 
		result = list_delete(suspendList, suspendListLength, 1, iProcessID);
	// ״̬��û�п���ɾ����
	if (result < 0)
		return -3;

	printf("--> Process %d stoped!\n", iProcessID);
	return 0;
}

// ���¿�ʼ����
// suspend -> ready
int Scheduler_ResumeProcess(int iProcessID)
{
	// ���̺�ΪiProcessID�Ľ��̲�����
	if (pcbListTimeCount[iProcessID] == -1 || iProcessID < 0 || iProcessID >= 512)
	{
		printf("--> Process %d can't be resumed! The process is not exist!\n", iProcessID);
		return -1;
	}

	int result = -1;
	// �ӹ������״̬��ɾ��
	result = list_delete(suspendList, suspendListLength, 1, iProcessID);
		
	if (result == 0)
	{
		// �������̬,�ж��Ƿ�Ϊ�����ڽ���
		// �������ڽ��̣�����priorityList,����β����
		if (pcbList[iProcessID].iPeriod != -1)
			list_insert(periodList, periodListLength, 0, periodListLength - 1, iProcessID);
		else // ��ͨ���ȼ����У��������ȼ�activatePQ����
			PQ_insert(activatePQ, activatePQLength, iProcessID);

		printf("--> Process %d resumed!\n", iProcessID);
	}
	else
		printf("--> Process %d can't be resumed! The process is not in suspended-list.\n", iProcessID);
	return 0;
}

// �����˳�������������
int Scheduler_ProcessExit(int iProcessID)
{
	printf("--> Process %d exit!\n", iProcessID);
	return 0;
}

/////////////////////////////////////////////
// ���н��̣���ά��pcbList
// ���������ø���
int RunProcess(int iProcessID)
{
	printf("--> Do Process %d started!\n", iProcessID);
	return 0;
}

// ������̣���ά��pcbList
//
// ��ע�⣡���˺���SuspendProcess�������ϸ������ϵĽ�������̬
// ���ǽ��̵���������Scheduler_Startʱ������ִ�еĽ��̷��ؽ������̬
//
// ���������ø���
int SuspendProcess(int iProcessID)
{
	printf("--> Do Process %d suspend!\n", iProcessID);
	return 0;
}


// ÿһ����ִ��һ�Σ��������л�"����"���̡�
// ��ע�⣡������ע�������Scheduler�в�Ӧ�ù�����̣�������running -> ready
int Scheduler()
{
	if (running != -1)
		printf("--> Begin schedule, Process %d is running now!\n", pcbRunning->iProcessID);
	else 
		printf("--> Begin schedule, No process is running!\n");
	///////////////�˴���������㷨//////////////////////

	// ����н����������У���¼ʣ��ʱ��
	if (running != -1 && pcbListTimeCount[running] > 0)
		pcbListTimeCount[running]--;
	
	int pid = -1;
	// �����о���״̬�е����ڽ��̣���¼ʱ��Ƭʣ��ʱ��
	for (int i = 0; i < periodListLength; i++)
	{	
		pid =  periodList[i];
		// ���ȳ������⣬����-1
		if (pid == -1 || pcbListTimeCount[pid] == -1)
		{	
			printf("-->Error: in period process!\n");
			continue;
		}
		// running�е��Ѿ���ǰ���¼����һ���ֵ
		if (pcbListTimeCount[pid] != 0 && pid != running)
			pcbListTimeCount[pid]--; // ��¼ʱ��Ƭʣ��ʱ��
	}

	// ��ʶ�Ƿ��Ѿ�ִ�е��ȣ�0��ʾ��δִ��
	int isScheduler = 0;

	// ����ִ�����ڽ���
	for (int i = 0; i < periodListLength; i++)
	{	
		pid =  periodList[i];
		// ���ȳ������⣬����-1
		if (pid == -1 || pcbListTimeCount[pid] == -1)
		{	
			printf("-->Error: in period process!\n");
			continue;
		}

		// �����ڽ��̵����������޻�û�н���������
		if (isScheduler == 0 && 
			// ���������еĽ���Ϊ���ڽ��̣����������ʱ��Ƭ����pcbListTimeCount[running] == 0
			((running != -1 && pcbListTimeCount[pid] == 0 && pcbList[running].iPeriod >= 0 && pcbListTimeCount[running] == 0)
			// ���������еĽ���Ϊ�����ڽ��̣�����ռ
			|| (running != -1 && pcbListTimeCount[pid] == 0 && pcbList[running].iPeriod == -1)
			|| running == -1
			))
		{
			// ������ռ����
			//
			// ��ע��!��
			// ���е�RunProcess��SuspendProcess��������Scheduler_ResumeProcess�����д���
			Scheduler_StartProcess(pid);
			isScheduler = 1;
		}
	}

	// ���ڽ��̴�������󣬴�����ͨ���ȼ�����
	pid = -1;
	// ���û������Scheduler_Start����������������ͨ���ȼ����̾���̬�е�������ȼ�����
	if (isScheduler == 0)
	{
		// ����ͨ���ȼ������л��д����еĽ���
		if (activatePQLength > 0)
			pid = activatePQ[0];
		// ����ͨ���ȼ�������û�д����еĽ��̣��������еĽ���ʱ��Ƭ��ִ���꣩����������ִ����Ľ���
		if (pid == -1 && expiredPQLength != 0 && pcbListTimeCount[running] == 0)
		{
			// ����activatePQ��expiredPQָ�뼰����ֵ
		
			//////////////////////////////////
			InitializeCriticalSection (&activeExpiredPQCS); ///��ʼ���ٽ������� 
			
			int * temp = activatePQ;
			activatePQ = expiredPQ;
			expiredPQ = temp;
			int tempLength = activatePQLength;
			activatePQLength = expiredPQLength;
			expiredPQLength = tempLength;

			//////////////////////////////////
			LeaveCriticalSection (&activeExpiredPQCS); ///�ͷ��ٽ�������
		
			if (activatePQLength > 0)
				pid = activatePQ[0]; 
		}
		// ���activate�������ȼ��ߵĽ����һ���û���������еĽ���
						// ���������еĽ����Ƿ����ڽ��̣��������ȼ����ߵĽ��̣�����ռ
		if (pid != -1 && ((running != -1 && pcbList[running].iPeriod == -1 && PQ_compare(running, pid) == -1)
						// ���������еĽ��������ʱ��Ƭ			
						|| (running != -1 && pcbListTimeCount[running] == 0) 
						|| running == -1))
		{	
			// ������ռ����
			//
			// ��ע��!��
			// ���е�RunProcess��SuspendProcess��������Scheduler_StartProcess�����д���
			Scheduler_StartProcess(pid);
			isScheduler = 1;
		}
	}

	// ����δ��������
	if (isScheduler == 0 && running != -1 && pcbListTimeCount[running] == 0)
		Scheduler_StartProcess(running);
	
	///////////////�����㷨����//////////////////////////
	if (running != -1)
		printf("--> End schedule, Process %d is running!\n", pcbRunning->iProcessID);
	else 
		printf("--> End schedule, No process is running!\n");
	return 0;
}
//////////////////////////////////////////////////
