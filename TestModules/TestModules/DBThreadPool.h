#ifndef __DB_CONNECTION_POOL
#define __DB_CONNECTION_POOL
#include "DBConnector.h"
#include "TLSObjectPool_IntrusiveList.hpp"


struct stBlock
{
	enum EBlock { SIZE = 128, };
	alignas(8) char mem[SIZE];
};
class CDBReadThreadPool
{
	enum endb
	{
		EXIT_SIGNAL = 0,
		REQUEST_QUERY = 1,
		RUNNING = 4,
		CREATE = 16,
	};
public:
	~CDBReadThreadPool();
	static bool Init(const char* hostIP, const char* id, const char* pw,
		const char* schema, unsigned short port);

	static CTlsMySqlConnector& GetConn();

	static char* alloc()
	{
		return (char*)s_objects.Alloc();
	}
	static int free(stBlock* object)
	{
		int ret = s_objects.Free(object);
		return ret;
	}
	static void RequestQuery(ULONG_PTR query);
private:
	CDBReadThreadPool();
	static unsigned int WorkerProc(void* param);
	static void Rollback();

	bool _isInit;
	CTlsMySqlConnector _conn;
	HANDLE _iocp;
	HANDLE _threads[CREATE];
	int32_t _createThreadNum;
	int32_t _exitThreadNum;
	uint32_t _requestNum;

	static CDBReadThreadPool s_pool;
	static CTlsObjectPool<stBlock, 0x00482233, TLS_OBJECTPOOL_USE_RAW> s_objects;
};


#endif