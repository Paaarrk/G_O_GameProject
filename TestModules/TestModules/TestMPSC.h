#pragma once
#include "LockFreeQueue_selected.hpp"
#include <thread>
#include <vector>
#include "logclassV1.h"

using namespace Core;

inline const wchar_t* TAG = L"Test_MPSC";
inline const wchar_t* TAG2 = L"Test_MPMC";
enum ETest_MPSC
{
	ENQ_DATA = 1,
	ENQ_THREAD_CNT = 6,
	ENQ_CNT_PER_THREAD = 10000'0000,
};
class CTest_MPSC
{
public:
	void EnqueueFunc()
	{
		DWORD tid = GetCurrentThreadId();
		printf_s("[%5d] Enqueue Thread Start! \n", tid);
		bool& flag = this->flag;
		CLockFreeQueue<int>& q = _q;
		while (!flag) _mm_pause();
		int data = (int)ENQ_DATA;
		for (int i = 0; i < ENQ_CNT_PER_THREAD; i++)
		{
			q.Enqueue_NotFail(data);
			if (i % 100 == 0 && rand() % 10 == 0)
				Sleep(1);
		}

		_InterlockedIncrement(&enqExitCnt);
		printf_s("[%5d] Enqueue Thread Exit! \n", tid);
		return;
	}

	void DequeueFunc()
	{
		DWORD tid = GetCurrentThreadId();
		printf_s("[%5d] Dequeue Thread Start! \n", tid);
		CLockFreeQueue<int>& q = _q;
		bool& flag = this->flag;
		long& enqExitCnt = this->enqExitCnt;
		while (!flag) _mm_pause();
		int count = 0;
		while (enqExitCnt < ENQ_THREAD_CNT)
		{
			int data;
			if (q.Dequeue_Single(data))
			{
				count++;
				if (data != ENQ_DATA)
				{
					c_syslog::logging().Log(TAG, c_syslog::en_ERROR, L"큐 꼬임", q.GetSize());
					__debugbreak();
				}
			}
		}

		while (q.GetSize())
		{
			int data;
			if (q.Dequeue_Single(data) == false)
			{
				c_syslog::logging().Log(TAG, c_syslog::en_ERROR, L"Getsize() %d 인데 디큐 실패", q.GetSize());
				__debugbreak();
			}
			if (data != ENQ_DATA)
			{
				c_syslog::logging().Log(TAG, c_syslog::en_ERROR, L"큐 꼬임", q.GetSize());
				__debugbreak();
			}
			count++;
		}

		if (count != ENQ_THREAD_CNT * ENQ_CNT_PER_THREAD)
		{
			c_syslog::logging().Log(TAG, c_syslog::en_ERROR, L"뽑다말음 : count %d / total %d", count, ENQ_THREAD_CNT * ENQ_CNT_PER_THREAD);
			__debugbreak();
		}
		printf_s("[deq / enq] %d / %d \n", count, ENQ_THREAD_CNT * ENQ_CNT_PER_THREAD);

		deqfin = true;
		printf_s("[%5d] Dequeue Thread Exit! \n", tid);
		return;
	}

	CTest_MPSC():flag(false), enqExitCnt(0), deqfin(false)
	{
		// 사전 준비
		for (int i = 0; i < ENQ_THREAD_CNT; i++)
		{
			_vThreads.emplace_back(&CTest_MPSC::EnqueueFunc, this);
		}
		_vThreads.emplace_back(&CTest_MPSC::DequeueFunc, this);

		// 시작
		flag = true;
	}

	~CTest_MPSC()
	{
		for (std::thread& th : _vThreads)
		{
			if (th.joinable())
				th.join();
		}
	}

	bool TryFin() const { return deqfin; }

	struct STInfo
	{
		int QueueSize;
		int ACreateChunk;
		int ALeftChunk;
	};

	void GetInfo(STInfo& info) const
	{
		info.QueueSize = _q.GetSize();
		info.ACreateChunk = _q.GetCreateChunkNum();
		info.ALeftChunk = _q.GetInPoolChunkNum();
	}

private:
	CLockFreeQueue<int> _q;
	std::vector<std::thread> _vThreads;
	bool flag;
	bool deqfin;
	long enqExitCnt;
};


enum ETest_MPMC
{
	ENQ_DATA_MPMC = 2,
	ENQ_THREAD_CNT_MPMC = 3,
	DEQ_THREAD_CNT_MPMC = 3,
	ENQ_CNT_PER_THREAD_MPMC = 10000'0000,
};
class CTest_MPMC
{
public:
	void EnqueueFunc()
	{
		DWORD tid = GetCurrentThreadId();
		printf_s("[%5d] Enqueue Thread Start! \n", tid);
		bool& flag = this->flag;
		CLockFreeQueue<int>& q = _q;
		while (!flag) _mm_pause();
		int data = (int)ENQ_DATA_MPMC;
		for (int i = 0; i < ENQ_CNT_PER_THREAD_MPMC; i++)
		{
			q.Enqueue_NotFail(data);
			if (i % 100 == 0 && rand() % 10 == 0)
				Sleep(1);
		}

		_InterlockedIncrement(&enqExitCnt);
		printf_s("[%5d] Enqueue Thread Exit! \n", tid);
		return;
	}

	void DequeueFunc()
	{
		DWORD tid = GetCurrentThreadId();
		printf_s("[%5d] Dequeue Thread Start! \n", tid);
		CLockFreeQueue<int>& q = _q;
		bool& flag = this->flag;
		long& enqExitCnt = this->enqExitCnt;
		while (!flag) _mm_pause();
		int count = 0;
		while (enqExitCnt < ENQ_THREAD_CNT_MPMC)
		{
			int data;
			if (q.Dequeue(data))
			{
				count++;
				if (data != ENQ_DATA_MPMC)
				{
					c_syslog::logging().Log(TAG2, c_syslog::en_ERROR, L"큐 꼬임", q.GetSize());
					__debugbreak();
				}
			}
		}

		int data;
		while (q.Dequeue(data))
		{
			if (data != ENQ_DATA_MPMC)
			{
				c_syslog::logging().Log(TAG2, c_syslog::en_ERROR, L"큐 꼬임", q.GetSize());
				__debugbreak();
			}
			count++;
		}

		printf_s("[deq / enq] %d / %d \n", count, ENQ_THREAD_CNT_MPMC * ENQ_CNT_PER_THREAD_MPMC);

		_InlineInterlockedAdd((long*) &totalCount, count);
		_InterlockedIncrement(&deqfin);
		printf_s("[%5d] Dequeue Thread Exit! \n", tid);
		return;
	}

	CTest_MPMC() :flag(false), enqExitCnt(0), deqfin(0), totalCount(0)
	{
		// 사전 준비
		for (int i = 0; i < ENQ_THREAD_CNT_MPMC; i++)
		{
			_vThreads.emplace_back(&CTest_MPMC::EnqueueFunc, this);
		}
		for (int i = 0; i < DEQ_THREAD_CNT_MPMC; i++)
		{
			_vThreads.emplace_back(&CTest_MPMC::DequeueFunc, this);
		}

		// 시작
		flag = true;
	}

	~CTest_MPMC()
	{
		if (totalCount != ENQ_THREAD_CNT_MPMC * ENQ_CNT_PER_THREAD_MPMC)
		{
			c_syslog::logging().Log(TAG2, c_syslog::en_ERROR, L"카운트 안맞음 (%d / %d)", totalCount, ENQ_THREAD_CNT_MPMC * ENQ_CNT_PER_THREAD_MPMC);
			__debugbreak();
		}
		for (std::thread& th : _vThreads)
		{
			if (th.joinable())
				th.join();
		}
	}

	bool TryFin() const { return deqfin == DEQ_THREAD_CNT_MPMC; }

	struct STInfo
	{
		int QueueSize;
		int ACreateChunk;
		int ALeftChunk;
	};

	void GetInfo(STInfo& info) const
	{
		info.QueueSize = _q.GetSize();
		info.ACreateChunk = _q.GetCreateChunkNum();
		info.ALeftChunk = _q.GetInPoolChunkNum();
	}

private:
	CLockFreeQueue<int> _q;
	std::vector<std::thread> _vThreads;
	bool flag;
	long deqfin;
	long enqExitCnt;
	int totalCount;
};

