#ifndef __DB_WRITER_H__
#define __DB_WRITER_H__
#include "DBProtocol.h"
#include "LockFreeQueue.hpp"
#include <thread>


class IDBWriterRequest;
class CDBWriterThread
{
	enum EDBWriterThread
	{
		SLEEP = 0,
		WAKE = 1,
	};
public:
	CDBWriterThread();
	void WriterProc();

	int32_t GetRequestNum() const { return _reqQ.GetSize(); }
private:
	CDBWriterThread(const CDBWriterThread&) = delete;
	CDBWriterThread(CDBWriterThread&&) = delete;
	CDBWriterThread& operator=(const CDBWriterThread&) = delete;
	std::thread _thread;
	std::atomic<bool> _wakeflag;
	Core::CLockFreeQueue<IDBWriterRequest*> _reqQ;
};


class IDBWriterRequest
{
public:
	IDBWriterRequest(uint64_t sessionId, const wchar_t* query)
		:_sessionId(sessionId), _query(query){ }
	
	virtual ~IDBWriterRequest(){ }
	virtual void WriteProcess() = 0;

	void* operator new(size_t size)
	{
		return DBBlock::Alloc();
	}
	void operator delete(void* p)
	{
		int ret = DBBlock::Free((char*)p);
		if (ret)
			__debugbreak();
	}
protected:
};


#endif
