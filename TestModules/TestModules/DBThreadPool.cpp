#include "DBThreadPool.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

CDBReadThreadPool CDBReadThreadPool::s_pool;

CDBReadThreadPool::CDBReadThreadPool() :_isInit(false), _iocp(NULL), _threads{}, _createThreadNum(0)
{

}

CDBReadThreadPool::~CDBReadThreadPool()
{

}

CTlsMySqlConnector& CDBReadThreadPool::GetConn()
{
	if (s_pool._isInit)
	{
		return s_pool._conn;
	}
	Log::logging().Log(TAG_DB, Log::en_ERROR, L"CDBReadThreadPool::GetConn(), Init 호출 안함");
	__debugbreak();
}

bool CDBReadThreadPool::Init(const char* hostIP, const char* id, const char* pw,
	const char* schema, unsigned short port)
{
	if (s_pool._isInit)
		return true;

	do
	{
		mysql_library_init(0, nullptr, nullptr);
		s_pool._conn.SetConnector(hostIP, id, pw, schema, port);

		s_pool._iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, RUNNING);
		if (s_pool._iocp == NULL)
		{
			Log::logging().Log(TAG_DB, Log::en_ERROR, L"Init(), CreateIoCompletionPort failed");
			break;
		}


	}while(0);



	return true;
}

void CDBReadThreadPool::Rollback()
{
	for (int i = 0; i < s_pool._createThreadNum; i++)
	{
		PostQueuedCompletionStatus(s_pool._iocp, EXIT_SIGNAL, 0, nullptr);
	}
	Sleep(100);
	for (int i = 0; i < CREATE; i++)
	{
		if (s_pool._threads[i] != 0)
		{
			CloseHandle(s_pool._threads[i]);
		}
	}
	s_pool._createThreadNum = 0;

	if (s_pool._iocp != NULL)
		CloseHandle(s_pool._iocp);

	mysql_library_end();
	s_pool._isInit = false;
}

unsigned int CDBReadThreadPool::WorkerProc(void* param)
{
	DWORD tid = GetCurrentThreadId();
	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Read Thread Start");
	HANDLE iocp = s_pool._iocp;

	while (1)
	{
		DWORD message = 0;

		// GetQueuedCompletionStatus(iocp, &message, )
		if (message == EXIT_SIGNAL)
		{
			break;
		}


		
	}


	Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"DB Read Thread End");
	return 0;
}