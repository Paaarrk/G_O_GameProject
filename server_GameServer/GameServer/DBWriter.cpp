#include "DBWriter.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

CDBWriterThread::CDBWriterThread():_isRunning(true)
{
	_thread = std::thread(&CDBWriterThread::WriterProc, this);
}

CDBWriterThread::~CDBWriterThread()
{
	Exit();

	if (_thread.joinable())
		_thread.join();
}

void CDBWriterThread::Exit()
{
	bool expected = true;
	if (_isRunning.compare_exchange_strong(expected, false, std::memory_order_seq_cst))
	{
		_wakeflag.exchange(static_cast<bool>(EDBWriterThread::WAKE));
		_wakeflag.notify_one();
	}
}

void CDBWriterThread::WriterProc()
{
	DWORD tid = GetCurrentThreadId();
	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Writer Thread Start");
	Core::CLockFreeQueue<IDBWriterRequest*>& q = _reqQ;
	IDBWriterRequest* pReq;
	while (_isRunning)
	{
		_wakeflag.wait(static_cast<bool>(EDBWriterThread::SLEEP), std::memory_order_seq_cst);
		
		while (q.Dequeue_Single(pReq) == true)
		{
			pReq->WriteProcess();
			delete pReq;
		}
		_wakeflag.exchange(SLEEP, std::memory_order_seq_cst);
		while (q.Dequeue_Single(pReq) == true)
		{
			pReq->WriteProcess();
			delete pReq;
		}
	}

	while (q.Dequeue_Single(pReq) == true)
	{
		pReq->WriteProcess();
		delete pReq;
	}



	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Writer Thread End");
}


void CDBWriterThread::Request(IDBWriterRequest* req)
{
	_reqQ.Enqueue_NotFail(req);
	_wakeflag.exchange(static_cast<bool>(EDBWriterThread::WAKE));
	_wakeflag.notify_one();
}