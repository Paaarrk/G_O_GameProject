#include "DBRequest.h"
#include "DBConnector.h"

std::vector<std::string> CDBRequest::Sync_GetWhiteIpList()
{
	std::vector<std::string> ret;

	CTlsMySqlConnector& conn = CDBReadThreadPool::GetConn();
	if (conn.RequestQuery(L"SELECT `ip` FROM `accountdb`.`whiteip`"))
	{
		
	}
	MYSQL_ROW row;
	MYSQL_RES* res = conn.GetResult();
	if (res)
	{
		while ((row = mysql_fetch_row(res))) 
		{
			ret.push_back(row[0]);
		}
		conn.FreeResult();
	}
	return ret;
}

std::vector<std::pair<int, int>> CDBRequest::Sync_GetCrystalData()
{
	std::vector<std::pair<int, int>> ret;

	CTlsMySqlConnector& conn = CDBReadThreadPool::GetConn();
	if (conn.RequestQuery(L"SELECT * FROM `gamedb`.`data_cristal`"))
	{
		
	}

	MYSQL_ROW row;
	MYSQL_RES* res = conn.GetResult();
	if (res)
	{
		while ((row = mysql_fetch_row(res))) 
		{
			ret.push_back({ std::stoi(row[0]), std::stoi(row[1])});
		}
		conn.FreeResult();
	}

	return ret;
}

std::vector<std::tuple<int, int, int>> CDBRequest::Sync_GetPlayerBaseData()
{
	std::vector<std::tuple<int, int, int>> ret;

	CTlsMySqlConnector& conn = CDBReadThreadPool::GetConn();
	if (conn.RequestQuery(L"SELECT * FROM `gamedb`.`data_player`"))
	{
		
	}

	MYSQL_ROW row;
	MYSQL_RES* res = conn.GetResult();
	if (res)
	{
		while ((row = mysql_fetch_row(res)))
		{
			ret.push_back({ std::stoi(row[0]), std::stoi(row[1]), std::stoi(row[2]) });
		}
		conn.FreeResult();
	}

	return ret;
}

std::vector<std::tuple<int, int, int>> CDBRequest::Sync_GetMonsterBaseData()
{
	std::vector<std::tuple<int, int, int>> ret;

	CTlsMySqlConnector& conn = CDBReadThreadPool::GetConn();
	if (conn.RequestQuery(L"SELECT * FROM `gamedb`.`data_monster`"))
	{
		
	}

	MYSQL_ROW row;
	MYSQL_RES* res = conn.GetResult();
	if (res)
	{
		while ((row = mysql_fetch_row(res)))
		{
			ret.push_back({ std::stoi(row[0]), std::stoi(row[1]), std::stoi(row[2]) });
		}
		conn.FreeResult();
	}

	return ret;
}

template<typename... Args>
void CDBRequest::Async_RequestSingleQuery(uint64_t sessionId, int reqtype, const wchar_t* wformat, Args&&... args)
{
	char* mem = CDBReadThreadPool::alloc();
	auto func = [ = reqtype, wfmt = wformat, ...capturedArgs = std::forward<Args>(args)](CTlsMySqlConnector& conn) {
		if(conn.RequestQuery(wfmt, capturedArgs...))
		{
			
		}
		
		MYSQL_ROW row;
		MYSQL_RES* res = conn.GetResult();
		if (res)
		{
			while ((row = mysql_fetch_row(res)))
			{
				
			}
		}

	};

	using RequestType = stDBReq<decltype(func)>;
	static_assert(sizeof(RequestType) <= stBlock::SIZE, "확장 필요");

	auto* request = new(mem) RequestType(std::move(func));
	CDBReadThreadPool::RequestQuery((ULONG_PTR)request);
}