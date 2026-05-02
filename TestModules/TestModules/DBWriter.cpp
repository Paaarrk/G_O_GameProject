#include "DBWriter.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

CDBWriterThread::CDBWriterThread()
{
	_thread = std::thread(&CDBWriterThread::WriterProc, this);
}

void CDBWriterThread::WriterProc()
{
	DWORD tid = GetCurrentThreadId();
	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Writer Thread Start");
	Core::CLockFreeQueue<IDBWriterRequest*>& q = _reqQ;
	IDBWriterRequest* pReq;
	while (1)
	{
		_wakeflag.wait(static_cast<bool>(EDBWriterThread::SLEEP), std::memory_order_seq_cst);
		
		while (q.Dequeue_Single(pReq) == true)
		{
			pReq->WriteProcess();
		}
		_wakeflag.exchange(SLEEP, std::memory_order_seq_cst);
		while (q.Dequeue_Single(pReq) == true)
		{
			pReq->WriteProcess();
		}
	}

	while (q.Dequeue_Single(pReq) == true)
	{
		pReq->WriteProcess();
	}



	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Writer Thread End");
}