#ifndef __DB_CONNECTION_POOL
#define __DB_CONNECTION_POOL
#include <process.h>
#include "LockFreeQueue.hpp"
#include "DBProtocol.h"

#include "logclassV1.h"
using Log = Core::c_syslog;



class IAsyncRequest
{
public:
	IAsyncRequest(EDBPoolUse who, uint64_t sessionId, const wchar_t* query)
		: _who(who), _sessionId(sessionId), _query(query) {
	}
	virtual ~IAsyncRequest() {};
	virtual void ProcessingQuery() = 0;
	virtual void ResponseProcess() = 0;

	void* operator new(size_t size)
	{
		return DBBlock::Alloc();
	}
	void operator delete(void* p)
	{
		int ret = DBBlock::Free((char*)p);
		if (ret) __debugbreak();
	}
	EDBPoolUse GetWho() const { return _who; }
protected:
	EDBPoolUse _who;
	uint64_t _sessionId;
	const wchar_t* _query;
};

template <int32_t usecount>
class CDBThreadPool
{
public:
	static CDBThreadPool<usecount>& GetDBThreadPool() 
	{
		static CDBThreadPool<usecount> pool;
		return pool;
	}
	~CDBThreadPool()
	{
		Rollback();
	}
	void RequestQuery(IAsyncRequest* req)
	{
		PostQueuedCompletionStatus(_iocp, EDBPool::REQUEST_QUERY,
			(ULONG_PTR)req, nullptr);
		_InterlockedIncrement(&_requestNum);
	}
	
	// nullptr: 데이터 없음 / notnull: 뽑기성공
	template<EDBPoolUse who>
	requires (0 <= who && who < usecount)
	IAsyncRequest* GetResponse()
	{
		IAsyncRequest* block;
		if (_returnQ[who].Dequeue(block))
		{
			return block;
		}

		return nullptr;
	}

	int GetRequestNum() { return _requestNum; }
	template<EDBPoolUse who>
	requires (0 <= who && who < usecount)
	int GetQueueSize() 
	{
		return _returnQ[who].GetSize(); 
	}
private:
	CDBThreadPool()
	{
		_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, EDBPool::RUNNING);
		if (_iocp == NULL)
			__debugbreak();

		for (int i = 0; i < EDBPool::CREATE; i++)
		{
			_createThreadNum++;
			_threads[i] = (HANDLE)_beginthreadex(NULL, NULL, CDBThreadPool::WorkerProc, NULL, 0, NULL);
			if (_threads[i] == NULL)
				__debugbreak();
		}
	}
	CDBThreadPool(CDBThreadPool&& ref) = delete;
	CDBThreadPool(const CDBThreadPool& ref) = delete;
	void operator=(const CDBThreadPool& ref) = delete;
	static unsigned int WorkerProc(void* param)
	{
		DWORD tid = GetCurrentThreadId();
		Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Read Thread Start");
		CDBThreadPool& s_pool = GetDBThreadPool();
		HANDLE iocp = s_pool._iocp;

		while (1)
		{
			DWORD message = -1;
			IAsyncRequest* req = nullptr;
			OVERLAPPED* ol;
			GetQueuedCompletionStatus(iocp, &message, (ULONG_PTR*)&req, &ol, INFINITE);
			if (message == EDBPool::EXIT_SIGNAL)
			{
				break;
			}
			else if (message == EDBPool::REQUEST_QUERY)
			{
				_InterlockedDecrement(&s_pool._requestNum);
				req->ProcessingQuery();
				if (0 <= req->GetWho() && req->GetWho() < usecount)
				{
					s_pool._returnQ[req->GetWho()].Enqueue_NotFail(req);
				}
				else
				{
					Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"Who type is Broken (%d)", req->GetWho());
					// delete req; 
				}
			}
		}

		_InterlockedIncrement((volatile long*)&s_pool._exitThreadNum);
		Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Read Thread End");
		return 0;
	}
	void Rollback()
	{
		for (int i = 0; i < _createThreadNum; i++)
		{
			PostQueuedCompletionStatus(_iocp, EXIT_SIGNAL, 0, nullptr);
		}

		while (_exitThreadNum != _createThreadNum)
			_mm_pause();

		for (int i = 0; i < CREATE; i++)
		{
			if (_threads[i] != 0)
			{
				CloseHandle(_threads[i]);
			}
		}
		_createThreadNum = 0;

		if (_iocp != NULL)
			CloseHandle(_iocp);
	}

	HANDLE _iocp = NULL;
	HANDLE _threads[EDBPool::CREATE] = {NULL, };
	int32_t _createThreadNum = 0;
	int32_t _exitThreadNum = 0;
	uint32_t _requestNum = 0;

	Core::CLockFreeQueue<IAsyncRequest*> _returnQ[usecount];
};

#endif