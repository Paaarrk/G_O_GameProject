#ifndef __DB_CONNECTION_POOL
#define __DB_CONNECTION_POOL
#include "DBConnector.h"

class CDBReadThreadPool
{
	enum ThreadNum
	{
		EXIT_SIGNAL = 0,
		RUNNING = 4,
		CREATE = 16,
	};
public:
	~CDBReadThreadPool();
	static bool Init(const char* hostIP, const char* id, const char* pw,
		const char* schema, unsigned short port);

	static CTlsMySqlConnector& GetConn();

	static unsigned int WorkerProc(void* param);

private:
	CDBReadThreadPool();
	static void Rollback();

	bool _isInit;
	CTlsMySqlConnector _conn;
	HANDLE _iocp;
	HANDLE _threads[CREATE];
	int32_t _createThreadNum;

	static CDBReadThreadPool s_pool;
};


#endif