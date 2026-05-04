#ifndef __DB_CONNECTOR_H__
#define __DB_CONNECTOR_H__

#include "DBProtocol.h"
#include "mysql.h"
#include "errmsg.h"
#pragma comment(lib, "mysqlclient.lib")

#include <mutex>

#include <timeapi.h>
#pragma comment(lib, "winmm")

#include "logclassV1.h"
using Log = Core::c_syslog;

#define MYSQL_MULTI_CONNECTION

template <EPhysicalInstance instance>
class CTlsMySqlConnector
{
public:
	enum enMySqlConnector
	{
		// 초반 활성화 시간은 제외하기위해서
		TIMING_REGISTER_SLOW_QUERY = 200,
		// 쿼리 widechar기준 최대 문자 수
		QUERY_SIZE = 2048,
		// n회마다 느린 쿼리 목록을 로그로 남기기
		TIMING_WHEN_WRITE_SLOW_SET = 1000000,
		// maxDeltaTime이 어느정도일때 즉시 남길지
		TIMING_HOW_MS_WRITE_SLOW_SET = 100
	};

	~CTlsMySqlConnector()
	{
		if (_id != nullptr)
			free(_id);
		if (_pw != nullptr)
			free(_pw);
		if (_schema != nullptr)
			free(_schema);
	}

	void SetConnector(const char* hostIP, const char* id, const char* pw,
		const char* schema, unsigned short port)
	{
		if (hostIP != nullptr)
			strcpy_s(_host, hostIP);

		if (id != nullptr)
		{
			size_t idlen = strlen(id) + 1;
			_id = (char*)malloc(idlen);
			if (_id == nullptr)
				__debugbreak();
			strcpy_s(_id, idlen, id);
		}

		if (pw != nullptr)
		{
			size_t pwlen = strlen(pw) + 1;
			_pw = (char*)malloc(pwlen);
			if (_pw == nullptr)
				__debugbreak();
			strcpy_s(_pw, pwlen, pw);
		}

		if (schema != nullptr)
		{
			size_t schemalen = strlen(schema) + 1;
			_schema = (char*)malloc(schemalen);
			if (_schema == nullptr)
				__debugbreak();
			strcpy_s(_schema, schemalen, schema);
		}

		_port = port;
	}

	//---------------------------------------
	// 성공시 0, 실패시 1 반환
	// tryCnt = 실패시 시도할 횟수
	// tick = 시도 간격, 0이면 쉬지 않음
	//---------------------------------------
	int Connect(int tryCnt = 3, DWORD tick = 10)
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			pConn = new stConn();
			mysql_init(&pConn->_conn);
			pConn->bInit = true;
			tls_conn = pConn;
		}

		MYSQL* connection = NULL;
		int cnt;
		for (cnt = 0; cnt < tryCnt; cnt++)
		{
#ifdef MYSQL_MULTI_CONNECTION
			connection = mysql_real_connect(&pConn->_conn,
				_host, _id, _pw, _schema, _port, NULL, CLIENT_MULTI_STATEMENTS);
#else
			connection = mysql_real_connect(&pConn->_conn,
				_host, _id, _pw, _schema, _port, NULL, 0);
#endif


			if (connection != NULL)
			{
				pConn->_connectCnt += cnt;
				break;
			}
			LogSQLError();
			Sleep(10);
			mysql_close(&pConn->_conn);
			memset(&pConn->_conn, 0, sizeof(MYSQL));
			mysql_init(&pConn->_conn);
		}
		if (connection == NULL)
			return 1;
		return 0;
	}

	//---------------------------------------
	// 멀티쿼리라면 사용자가 직접 조립하고주는게 맞지않나?
	// 성공시 0, 실패시 0아님
	// 실패 1: DB서버문제
	// 실패 2: 쿼리 문제
	//---------------------------------------
	int RequestQuery(const wchar_t* wformat, ...)
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			pConn = new stConn();
			mysql_init(&pConn->_conn);
			pConn->bInit = true;
			tls_conn = pConn;
			Connect();
		}

		va_list args;
		va_start(args, wformat);
		vswprintf_s(pConn->_curWQuery, QUERY_SIZE, wformat, args);
		va_end(args);
		WideCharToMultiByte(CP_UTF8, 0, pConn->_curWQuery, -1, pConn->_curAQuery, QUERY_SIZE * 4, 0, 0);

		if (mysql_ping(&pConn->_conn) != 0)
		{
			if (Connect() != 0)
			{
				LogSQLError();
				return 1;
			}
		}
		DWORD startTime = timeGetTime();
		DWORD endTime;
		uint64_t deltaTime;
		if (mysql_query(&pConn->_conn, pConn->_curAQuery) == 0)
		{
			endTime = timeGetTime();
			deltaTime = (endTime - startTime);
			pConn->_avgTime = (pConn->_avgTime * pConn->_count + deltaTime) / (pConn->_count + 1);
			if (deltaTime > pConn->_maxDeltaTime)
				pConn->_maxDeltaTime = deltaTime;
			if (deltaTime >= TIMING_HOW_MS_WRITE_SLOW_SET)
			{
				Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"[** WARN **] [%s, %dms]",
					pConn->_curWQuery, deltaTime);
			}
			RegisterSlowQuery(pConn->_curWQuery, deltaTime);
			++pConn->_count;
			return 0;
		}

		unsigned int err = mysql_errno(&pConn->_conn);
		LogSQLError();
		if (err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST)
			return 1;

		Log::logging().Log(TAG_DB, Log::en_ERROR, L"Context Error: (Query) %s", pConn->_curWQuery);
		return 2;
	}

	unsigned int GetErrno() const
	{ 
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
			return 0;
		return mysql_errno(&pConn->_conn); 
	}

	//---------------------------------------
	// 응답 가져오기 / FreeResult()필수
	//---------------------------------------
	MYSQL_RES* GetResult()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return NULL;
		}
		pConn->_sql_result = mysql_store_result(&pConn->_conn);
		return pConn->_sql_result;
	}

	//---------------------------------------
	// 다음 응답 가져오기 / FreeResult()필수
	//---------------------------------------
	MYSQL_RES* GetNextResult()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return NULL;
		}
		if (mysql_next_result(&pConn->_conn) == 0)
		{
			pConn->_sql_result = mysql_store_result(&pConn->_conn);
			return pConn->_sql_result;
		}
		else
			return nullptr;
	}

	long long MySqlAffectedRows()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return -1;
		}
		return mysql_affected_rows(&pConn->_conn);
	}

	int MySqlNextResult() const
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr) return -1;
		return mysql_next_result(&pConn->_conn);
	}

	//---------------------------------------
	// 응답 해제하기
	//---------------------------------------
	void FreeResult()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return;
		}
		mysql_free_result(pConn->_sql_result);
	}

	//---------------------------------------
	// 로그 sql 에러
	//---------------------------------------
	void LogSQLError()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return;
		}
		wchar_t error[512];
		unsigned int errorNo = mysql_errno(&pConn->_conn);
		MultiByteToWideChar(CP_UTF8, 0, mysql_error(&pConn->_conn), -1, error, _countof(error));

		Log::logging().Log(TAG_DB, Log::en_ERROR,
			L"RequestQuery MySql Error(%u), %s", errorNo, error);
	}

	void ReleaseConn()
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return;
		}

		mysql_close(&pConn->_conn);

		WriteSlowQuery();

		delete pConn;
		tls_conn = nullptr;
	}

	struct stQueryInfo
	{
		uint64_t deltaTime = 0;
		wchar_t query[QUERY_SIZE] = {};
	};

	void RegisterSlowQuery(const wchar_t* wQuery, uint64_t deltaTime)
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return;
		}

		if (++(pConn->_querySendCnt) < TIMING_REGISTER_SLOW_QUERY)
		{
			return;
		}

		uint64_t flag = 0xFFFF'FFFF'FFFF'FFFF;
		int minIndex = -1;
		for (int i = 0; i < 5; ++i)
		{
			if (pConn->_slowQueryTop5[i].deltaTime < flag)
			{
				flag = pConn->_slowQueryTop5[i].deltaTime;
				minIndex = i;
			}
		}

		stQueryInfo* pQi = &pConn->_slowQueryTop5[minIndex];
		if (pQi->deltaTime < deltaTime)
		{
			pQi->deltaTime = deltaTime;
			wcscpy_s(pQi->query, QUERY_SIZE, wQuery);
		}

		WriteSlowQuery(true);
	}

	void WriteSlowQuery(bool useTiming = false)
	{
		stConn* pConn = tls_conn;
		if (pConn == nullptr)
		{
			return;
		}

		if (useTiming && (pConn->_querySendCnt % TIMING_WHEN_WRITE_SLOW_SET != 0))
		{
			return;
		}

		for (int i = 0; i < 5; i++)
		{
			stQueryInfo* pQi = &pConn->_slowQueryTop5[i];
			if (pQi->deltaTime > 0)
			{
				Log::logging().Log(TAG_DB, Log::en_SYSTEM, L"[%s, %dms]",
					pQi->query, pQi->deltaTime);
			}
		}
	}

	struct stConn
	{
		MYSQL _conn = {};
		MYSQL_RES* _sql_result = nullptr;
		bool bInit = false;

		uint64_t _avgTime = 0;
		uint64_t _maxDeltaTime = 0;
		uint64_t _count = 0;
		stQueryInfo _slowQueryTop5[5] = {};
		int _connectCnt = 0;
		int _querySendCnt = 0;
		wchar_t _curWQuery[QUERY_SIZE] = {};
		char _curAQuery[QUERY_SIZE * 4] = {};	//최대 2바이트가 4바이트로 바뀌어서
	};
	CTlsMySqlConnector() {}
private:
	CTlsMySqlConnector(const CTlsMySqlConnector&) = delete;
	CTlsMySqlConnector& operator=(const CTlsMySqlConnector&) = delete;

	char _host[16] = {};
	char* _id = nullptr;
	char* _pw = nullptr;
	char* _schema = nullptr;
	unsigned short _port = 0;
	inline static thread_local stConn* tls_conn;
};

template <EPhysicalInstance instance>
inline static CTlsMySqlConnector<instance> g_connector;

template <EPhysicalInstance instance>
static CTlsMySqlConnector<instance>& GetConnector()
{
	return g_connector<instance>;
}

#endif