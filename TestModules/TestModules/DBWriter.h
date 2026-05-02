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
	~CDBWriterThread();
	void Exit();
	void WriterProc();
	void Request(IDBWriterRequest* req);

	int32_t GetRequestNum() const { return _reqQ.GetSize(); }
private:
	CDBWriterThread(const CDBWriterThread&) = delete;
	CDBWriterThread(CDBWriterThread&&) = delete;
	CDBWriterThread& operator=(const CDBWriterThread&) = delete;
	std::thread _thread;
	std::atomic<bool> _wakeflag;
	std::atomic<bool> _isRunning;
	Core::CLockFreeQueue<IDBWriterRequest*> _reqQ;
};



class IDBWriterRequest
{
public:
	IDBWriterRequest() {}

	virtual ~IDBWriterRequest() {}
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
private:
};
#endif
