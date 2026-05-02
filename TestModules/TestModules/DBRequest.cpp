#include "DBRequest.h"
#include "DBConnector.hpp"

std::vector<std::string> CDBRequest::Sync_GetWhiteIpList()
{
	std::vector<std::string> ret;

	CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
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

	CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
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

	CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
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

	CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
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

