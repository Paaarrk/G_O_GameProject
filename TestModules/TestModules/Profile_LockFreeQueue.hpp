#pragma once

#ifdef SELECTED
#include "LockFreeQueue_selected.hpp"
#endif
#ifdef NEW1
#include "LockFreeQueue_new.hpp"
#else
#ifdef NEW2
#include "LockFreeQueue_new2.hpp"
#else
#ifdef NEW3
#include "LockFreeQueue_new3.hpp"
#else
#ifndef MPSC
#include "LockFreeQueue.hpp"
#else
#include "LockFreeQueue_selected.hpp"
#endif
#endif	
#endif
#endif

#include <stdio.h>
#include <thread>
#include <vector>
#include <profileapi.h>
using namespace Core;

enum EPfQueue
{
	ENQ_TH_CNT = 4,
	DEQ_TH_CNT = 4,

	ENQ_CNT_PER_TH = 1'0000'0000,
};


struct STPfInfo
{
	double totalTimeMs;
	double avgTimeUs;
	long long totalCount;
};

class CPfQueue
{
public:
	void EnqueueFunc()
	{
		long index = _InterlockedIncrement(&_getEnqIndex);
		CLockFreeQueue<long long>& q = _q;

		LARGE_INTEGER start;
		LARGE_INTEGER end;
		LARGE_INTEGER freq;
		while (!_start) _mm_pause();

		QueryPerformanceCounter(&start);
		for(long long i = 0; i < (long long)ENQ_CNT_PER_TH; i++)
		{
			q.Enqueue_NotFail(i);
		}
		QueryPerformanceCounter(&end);
		QueryPerformanceFrequency(&freq);

		STPfInfo& myInfo = _enqInfo[index];
		myInfo.totalCount = ENQ_CNT_PER_TH;
		myInfo.totalTimeMs = (double)(end.QuadPart - start.QuadPart) / (freq.QuadPart) * 1000;	//ms
		myInfo.avgTimeUs = myInfo.totalTimeMs * 1000 / ENQ_CNT_PER_TH;

		_InterlockedIncrement(&_enqQuitCnt);
	}

	void DequeueFunc()
	{
		long index = _InterlockedIncrement(&_getDeqIndex);
		CLockFreeQueue<long long>& q = _q;

		LARGE_INTEGER start;
		LARGE_INTEGER end;
		LARGE_INTEGER freq;
		long long count = 0;

		while (!_start) _mm_pause();

		QueryPerformanceCounter(&start);
		long long data;
		while (_enqQuitCnt < ENQ_TH_CNT)
		{
			if (q.Dequeue(data))
				++count;
		}

		while (q.Dequeue(data))
		{
			++count;
		}

		QueryPerformanceCounter(&end);
		QueryPerformanceFrequency(&freq);

		STPfInfo& myInfo = _deqInfo[index];
		myInfo.totalCount = count;
		myInfo.totalTimeMs = (double)(end.QuadPart - start.QuadPart) / (freq.QuadPart) * 1000;	//ms
		myInfo.avgTimeUs = myInfo.totalTimeMs * 1000 / count;
	}

	CPfQueue():_enqQuitCnt(0), _getEnqIndex(-1), _getDeqIndex(-1),
		_enqInfo{}, _deqInfo{}, _start(0)
	{
		// 메모리 예열부터
		for (int i = 0; i < ENQ_CNT_PER_TH / 1000; i++)
			_q.Enqueue(0);
		long long data;
		while (_q.Dequeue(data));

		HANDLE hProcess = GetCurrentProcess();
		SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);

		for(int i = 0; i < ENQ_TH_CNT; i++)
			_vEnqThreads.emplace_back(&CPfQueue::EnqueueFunc, this);
		for (int i = 0; i < DEQ_TH_CNT; i++)
			_vDeqThreads.emplace_back(&CPfQueue::DequeueFunc, this);
		DWORD core = 0;
		for (std::thread& th : _vEnqThreads)
		{
			HANDLE handle = th.native_handle();
			SetThreadAffinityMask(handle, core);
			SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
			++core;
		}
		for (std::thread& th : _vDeqThreads)
		{
			HANDLE handle = th.native_handle();
			SetThreadAffinityMask(handle, core);
			SetThreadPriority(handle, THREAD_PRIORITY_TIME_CRITICAL);
			++core;
		}

		_start = 1;


		for (std::thread& th : _vEnqThreads)
		{
			if (th.joinable())
				th.join();
		}
		for (std::thread& th : _vDeqThreads)
		{
			if (th.joinable())
				th.join();
		}

		// 취합

		Show();
	}

	void Show()
	{
		STPfInfo totalEnqInfo = {};
		STPfInfo totalDeqInfo = {};
		printf_s("Per Threads Enqueue --------------------------\n");
		for (int i = 0; i < ENQ_TH_CNT; i++)
		{
			STPfInfo& curInfo = _enqInfo[i];
			totalEnqInfo.totalCount += curInfo.totalCount;
			totalEnqInfo.totalTimeMs += curInfo.totalTimeMs;
			printf_s("[Thread %2d] | count: %10lld | time: %10.2lfms | avg: %10.2lfus | \n", i, curInfo.totalCount, curInfo.totalTimeMs, curInfo.avgTimeUs);
		}
		totalEnqInfo.avgTimeUs = totalEnqInfo.totalTimeMs * 1000 / totalEnqInfo.totalCount;
		printf_s("\n[    Total] | count: %10lld | time: %10.2lfms | avg: %10.2lfus | \n", totalEnqInfo.totalCount, totalEnqInfo.totalTimeMs, totalEnqInfo.avgTimeUs);
		printf_s("\n\nPer Threads Dequeue --------------------------\n");
		for (int i = 0; i < DEQ_TH_CNT; i++)
		{
			STPfInfo& curInfo = _deqInfo[i];
			totalDeqInfo.totalCount += curInfo.totalCount;
			totalDeqInfo.totalTimeMs += curInfo.totalTimeMs;
			printf_s("[Thread %2d] | count: %10lld | time: %10.2lfms | avg: %10.2lfus | \n", i, curInfo.totalCount, curInfo.totalTimeMs, curInfo.avgTimeUs);
		}
		totalDeqInfo.avgTimeUs = totalDeqInfo.totalTimeMs * 1000 / totalDeqInfo.totalCount;
		printf_s("\n[    Total] | count: %10lld | time: %10.2lfms | avg: %10.2lfus | \n", totalDeqInfo.totalCount, totalDeqInfo.totalTimeMs, totalDeqInfo.avgTimeUs);
		printf_s("\nLeftQueueSize: %d \n", _q.GetSize());
	}

	~CPfQueue()
	{

	}
private:
	std::vector<std::thread> _vEnqThreads;
	std::vector<std::thread> _vDeqThreads;
	long _getEnqIndex;
	long _getDeqIndex;
	long _start;
	alignas(64) CLockFreeQueue<long long> _q;
	alignas(64) long _enqQuitCnt;
	STPfInfo _enqInfo[ENQ_TH_CNT];
	STPfInfo _deqInfo[DEQ_TH_CNT];
};