#include <stdio.h>
#include <set>
#include <process.h>
#include "_CrashDump.hpp"
Core::CCrashDump g_dump;

// #define NEW2
// #define NEW1
// #define NEW3
#ifdef NEW1
#include "LockFreeQueue_new.hpp"
#else
	#ifdef NEW2
	#include "LockFreeQueue_new2.hpp"
	#else
		#ifdef NEW3
		#include "LockFreeQueue_new3.hpp"
		#else
		#include "LockFreeQueue.hpp"
		#endif	
	#endif
#endif
#include "Profile_LockFreeQueue.hpp"
#include <conio.h>
using namespace Core;

#define ENQUEUE_COUNT			1'0000'0000
#define ENQUEUE_THREAD_COUNT	4
#define DEQUEUE_THREAD_COUNT	4


struct STCorrectTestParams
{
	std::set<long long>*			pDeqSet;
	unsigned int*					pStopFlag;
	CLockFreeQueue<long long>*		pQueue;
	long long*						pGlobalCount;
};

struct STCorrectTestParams2
{
	long long*						pDeqSet;
	unsigned int*					pStopFlag;
	CLockFreeQueue<long long>*		pQueue;
	long long*						pGlobalCount;
};

//---------------------------------------------------
// CorrectTest1 (MPMC)
// . set을 통해 제대로 넣고 빼는지
// . 데이터 중복/누락이 없는지
//---------------------------------------------------
void			CorrectTest1();
unsigned int	CorrectTest_EnqueueFunc(void* param);
unsigned int	CorrectTest_DequeueFunc(void* param);

//---------------------------------------------------
// CorrectTest2 (MPMC)
// . 카운팅이 맞는지
//---------------------------------------------------
void			CorrectTest2();
unsigned int	CorrectTest_EnqueueFunc2(void* param);
unsigned int	CorrectTest_DequeueFunc2(void* param);


int main(void)
{
	// CorrectTest1();
	// CorrectTest2();
	CPfQueue queue;
	int key = _getch();
	return 0;
}



void CorrectTest1()
{
	printf_s("CorrectTest1 Start! \n");
	

	std::set<long long> testSet1;
	std::set<long long> perThreadSet1[DEQUEUE_THREAD_COUNT];
	CLockFreeQueue<long long> queue1;
	STCorrectTestParams enqParams1[ENQUEUE_THREAD_COUNT];
	STCorrectTestParams deqParams1[DEQUEUE_THREAD_COUNT];

	std::set<long long> testSet2;
	std::set<long long> perThreadSet2[DEQUEUE_THREAD_COUNT];
	CLockFreeQueue<long long> queue2;
	STCorrectTestParams enqParams2[ENQUEUE_THREAD_COUNT];
	STCorrectTestParams deqParams2[DEQUEUE_THREAD_COUNT];

	alignas(64) long long globalcount1 = -1;
	alignas(64) unsigned int flag1 = 0;
	alignas(64) long long globalcount2 = -1;
	alignas(64) unsigned int flag2 = 0;

	for (long long i = 0; i < (long long)ENQUEUE_COUNT * ENQUEUE_THREAD_COUNT; i++)
	{
		testSet1.insert(i);
		testSet2.insert(i);
		if (i % 100'0000 == 0)
			printf_s("insert %lld \n", i);
	}

	for (int i = 0; i < ENQUEUE_THREAD_COUNT; i++)
	{
		STCorrectTestParams& ref1 = enqParams1[i];
		ref1.pGlobalCount = &globalcount1;
		ref1.pQueue = &queue1;
		ref1.pStopFlag = &flag1;

		STCorrectTestParams& ref2 = enqParams2[i];
		ref2.pGlobalCount = &globalcount2;
		ref2.pQueue = &queue2;
		ref2.pStopFlag = &flag2;
	}

	for (int i = 0; i < DEQUEUE_THREAD_COUNT; i++)
	{
		STCorrectTestParams& ref1 = deqParams1[i];
		ref1.pDeqSet = &perThreadSet1[i];
		ref1.pGlobalCount = &globalcount1;
		ref1.pQueue = &queue1;
		ref1.pStopFlag = &flag1;

		STCorrectTestParams& ref2 = deqParams2[i];
		ref2.pDeqSet = &perThreadSet2[i];
		ref2.pGlobalCount = &globalcount2;
		ref2.pQueue = &queue2;
		ref2.pStopFlag = &flag2;
	}

	// 스레드 생성 및 대기
	HANDLE threads[ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2];
	for (int i = 0; i < ENQUEUE_THREAD_COUNT * 2; i++)
	{
		if (i < ENQUEUE_THREAD_COUNT)
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_EnqueueFunc, &enqParams1[i], 0, NULL);
		else
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_EnqueueFunc, &enqParams2[i - ENQUEUE_THREAD_COUNT], 0, NULL);
	}
	for (int i = ENQUEUE_THREAD_COUNT * 2; i < ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2; i++)
	{
		if (i < ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT)
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_DequeueFunc, &deqParams1[i - ENQUEUE_THREAD_COUNT * 2], 0, NULL);
		else
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_DequeueFunc, &deqParams2[i - ENQUEUE_THREAD_COUNT * 2 - DEQUEUE_THREAD_COUNT], 0, NULL);
	}

	WaitForMultipleObjects(ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2, threads, TRUE, INFINITE);

	// 검ㅇ증
	long long set1count = 0;
	long long set2count = 0;
	for (int i = 0; i < DEQUEUE_THREAD_COUNT; i++)
	{
		auto& perset1 = perThreadSet1[i];
		auto& perset2 = perThreadSet2[i];
		for (long long val : perset1)
		{
			size_t ret = testSet1.erase(val);
			if (ret == 0)
			{
				set1count++;
				printf_s("set1 %lld 없음 \n", val);
			}
		}

		for (long long val : perset2)
		{
			size_t ret = testSet2.erase(val);
			if (ret == 0)
			{
				set2count++;
				printf_s("set2 %lld 없음 \n", val);
			}
		}
	}

	if (testSet1.size() != 0)
		__debugbreak();
	if (testSet2.size() != 0)
		__debugbreak();
	printf_s("1: %lld / 2: %lld \n", set1count, set2count);
	printf_s("1: %d / 2: %d \n ", queue1.GetSize(), queue2.GetSize());
	printf_s("CorrectTest1 End!   \n");
}

unsigned int CorrectTest_EnqueueFunc(void* param)
{
	DWORD tid = GetCurrentThreadId();
	printf_s("Enqueue Thread Start : %u \n", tid);

	STCorrectTestParams& params = *reinterpret_cast<STCorrectTestParams*>(param);
	unsigned int* pFlag = params.pStopFlag;
	CLockFreeQueue<long long>& queue = *params.pQueue;
	long long* pGlobalCount = params.pGlobalCount;
	
	for (long long i = ENQUEUE_COUNT; i > 0; --i)
	{
		long long data = _InterlockedIncrement64(pGlobalCount);
		queue.Enqueue_NotFail(data);
	}

	_InterlockedIncrement(pFlag);
	printf_s("Enqueue Thread End : %u   \n", tid);
	return 0;
}

unsigned int CorrectTest_DequeueFunc(void* param)
{
	DWORD tid = GetCurrentThreadId();
	printf_s("Dequeue Thread Start : %u \n", tid);

	STCorrectTestParams& params = *reinterpret_cast<STCorrectTestParams*>(param);
	std::set<long long>& deqSet = *params.pDeqSet;
	unsigned int& flag = *params.pStopFlag;
	CLockFreeQueue<long long>& queue = *params.pQueue;

	while(flag < ENQUEUE_THREAD_COUNT)
	{
		long long data = -1;
		if (queue.Dequeue(data))
		{
			auto result = deqSet.insert(data);
			if (result.second == false)
				__debugbreak();
		}
	}

	for(;;)
	{
		long long data;
		if (queue.Dequeue(data) == false)
			break;
		auto result = deqSet.insert(data);
		if (result.second == false)
			__debugbreak();
	}

	printf_s("Dequeue Thread End : %u   \n", tid);
	return 0;
}



void CorrectTest2()
{
	printf_s("CorrectTest2 Start! \n");

	long long perThreadDeqCnt1[DEQUEUE_THREAD_COUNT] = {};
	CLockFreeQueue<long long> queue1;
	STCorrectTestParams2 enqParams1[ENQUEUE_THREAD_COUNT];
	STCorrectTestParams2 deqParams1[DEQUEUE_THREAD_COUNT];

	long long perThreadDeqCnt2[DEQUEUE_THREAD_COUNT] = {};
	CLockFreeQueue<long long> queue2;
	STCorrectTestParams2 enqParams2[ENQUEUE_THREAD_COUNT];
	STCorrectTestParams2 deqParams2[DEQUEUE_THREAD_COUNT];

	alignas(64) long long globalcount1 = -1;
	alignas(64) unsigned int flag1 = 0;
	alignas(64) long long globalcount2 = -1;
	alignas(64) unsigned int flag2 = 0;

	for (int i = 0; i < ENQUEUE_THREAD_COUNT; i++)
	{
		STCorrectTestParams2& ref1 = enqParams1[i];
		ref1.pGlobalCount = &globalcount1;
		ref1.pQueue = &queue1;
		ref1.pStopFlag = &flag1;

		STCorrectTestParams2& ref2 = enqParams2[i];
		ref2.pGlobalCount = &globalcount2;
		ref2.pQueue = &queue2;
		ref2.pStopFlag = &flag2;
	}

	for (int i = 0; i < DEQUEUE_THREAD_COUNT; i++)
	{
		STCorrectTestParams2& ref1 = deqParams1[i];
		ref1.pDeqSet = &perThreadDeqCnt1[i];
		ref1.pGlobalCount = &globalcount1;
		ref1.pQueue = &queue1;
		ref1.pStopFlag = &flag1;

		STCorrectTestParams2& ref2 = deqParams2[i];
		ref2.pDeqSet = &perThreadDeqCnt2[i];
		ref2.pGlobalCount = &globalcount2;
		ref2.pQueue = &queue2;
		ref2.pStopFlag = &flag2;
	}

	// 스레드 생성 및 대기
	HANDLE threads[ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2];
	for (int i = 0; i < ENQUEUE_THREAD_COUNT * 2; i++)
	{
		if (i < ENQUEUE_THREAD_COUNT)
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_EnqueueFunc2, &enqParams1[i], 0, NULL);
		else
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_EnqueueFunc2, &enqParams2[i - ENQUEUE_THREAD_COUNT], 0, NULL);
	}
	for (int i = ENQUEUE_THREAD_COUNT * 2; i < ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2; i++)
	{
		if (i < ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT)
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_DequeueFunc2, &deqParams1[i - ENQUEUE_THREAD_COUNT * 2], 0, NULL);
		else
			threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CorrectTest_DequeueFunc2, &deqParams2[i - ENQUEUE_THREAD_COUNT * 2 - DEQUEUE_THREAD_COUNT], 0, NULL);
	}

	WaitForMultipleObjects(ENQUEUE_THREAD_COUNT * 2 + DEQUEUE_THREAD_COUNT * 2, threads, TRUE, INFINITE);

	// 검ㅇ증
	long long set1count = 0;
	long long set2count = 0;
	for (int i = 0; i < DEQUEUE_THREAD_COUNT; i++)
	{
		auto& perset1 = perThreadDeqCnt1[i];
		auto& perset2 = perThreadDeqCnt2[i];
		set1count += perset1;
		set2count += perset2;
	}

	printf_s("1: %lld / 2: %lld \n", set1count, set2count);
	printf_s("1: %d / 2: %d \n ", queue1.GetSize(), queue2.GetSize());
	printf_s("CorrectTest2 End!   \n");
}

unsigned int CorrectTest_EnqueueFunc2(void* param)
{
	DWORD tid = GetCurrentThreadId();
	printf_s("Enqueue Thread Start : %u \n", tid);

	STCorrectTestParams2& params = *reinterpret_cast<STCorrectTestParams2*>(param);
	unsigned int* pFlag = params.pStopFlag;
	CLockFreeQueue<long long>& queue = *params.pQueue;
	long long* pGlobalCount = params.pGlobalCount;

	for (long long i = ENQUEUE_COUNT; i > 0; --i)
	{
		long long data = _InterlockedIncrement64(pGlobalCount);
		queue.Enqueue_NotFail(data);
	}

	_InterlockedIncrement(pFlag);
	printf_s("Enqueue Thread End : %u   \n", tid);
	return 0;
}

unsigned int CorrectTest_DequeueFunc2(void* param)
{
	DWORD tid = GetCurrentThreadId();
	printf_s("Dequeue Thread Start : %u \n", tid);

	STCorrectTestParams2& params = *reinterpret_cast<STCorrectTestParams2*>(param);
	long long& deqSet = *params.pDeqSet;
	unsigned int& flag = *params.pStopFlag;
	CLockFreeQueue<long long>& queue = *params.pQueue;

	while (flag < ENQUEUE_THREAD_COUNT)
	{
		long long data = -1;
		if (queue.Dequeue(data))
		{
			++deqSet;
		}
	}

	for (;;)
	{
		long long data;
		if (queue.Dequeue(data) == false)
			break;
		++deqSet;
	}

	printf_s("Dequeue Thread End : %u   \n", tid);
	return 0;
}


