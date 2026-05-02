#ifndef __DB_REQUEST_H__
#define __DB_REQUEST_H__

#include "DBProtocol.h"
#include "DBConnector.hpp"
#include "DBThreadPool.h"
#include "DBWriter.h"
#include <vector>
#include <string>
#include <tuple>

class CDBRequest
{
public:
	//--------------------------------------------------------------------------
	// 동기 함수이므로 간단하게 호출 위치에서 커넥터를 사용함
	//--------------------------------------------------------------------------
	// ipv4 string
	static std::vector<std::string> Sync_GetWhiteIpList();
	// type, cnt pair
	static std::vector<std::pair<int, int>> Sync_GetCrystalData();
	// player data (damage, hp, recovery_hp)
	static std::vector<std::tuple<int, int, int>> Sync_GetPlayerBaseData();
	// type, hp, damage
	static std::vector<std::tuple<int, int, int>> Sync_GetMonsterBaseData();
private:

};

template <size_t size, size_t limit>
struct CheckSize;

constexpr int NAME_LEN_20 = 20;
struct stName
{
#pragma warning(push)
#pragma warning(disable : 26495) // 초기화 일부러 안함
	stName() { name[0] = '\0'; }
#pragma warning(pop)	
	stName(const char* p)
	{
		strcpy_s(name, NAME_LEN_20, p);
	}
	char name[NAME_LEN_20];
};
enum ERequestType
{
	ASYNC_GET_ACCOUNT_INFO,
	ASYNC_GET_CHARACTER_INFO,
};
inline static const wchar_t* QUERY_FORMATS[] = {
	L"SELECT `accountno`, `userId`, `usernick`, `status` FROM `accountdb`.`v_account` WHERE `accountno` = %lld",
	L"SELECT * FROM `gamedb`.`character` WHERE `accountno` = %lld"
};
// | int64_t accoutno | char[20] userid | char[20] usernick | int32_t status |
using GetAccountInfoResType = std::tuple<int64_t, stName, stName, int32_t>;
// | int64_t accoutno | int32_t charactertype | float posx | float posy | int32_t tilex | int32_t tiley | int32_t rotation | int32_t cristal | int32_t hp | int64_t exp | int32_t level | int32_t die |
using GetCharacterInfoResType = std::tuple <int64_t, int32_t, float, float, int32_t, int32_t, int32_t, int32_t, int32_t, int64_t, int32_t, int32_t>;


//-----------------------------------------------------
// READ
//-----------------------------------------------------

template <typename F>
concept AccountInfoFunc = std::invocable<F, const GetAccountInfoResType&>;
template<AccountInfoFunc Lambda>
class CAsync_GetAccountInfo : public IAsyncRequest
{
public:
	CAsync_GetAccountInfo(EDBPoolUse who, uint64_t sessionId, int64_t accountNo, Lambda&& func)
		: IAsyncRequest(who, sessionId, QUERY_FORMATS[ASYNC_GET_ACCOUNT_INFO]),
		_accountno(accountNo), _processfunction(std::forward<Lambda>(func)) {
		if constexpr (sizeof(CAsync_GetAccountInfo<Lambda>) > DBBlock::SIZE)
		{
			CheckSize<sizeof(CAsync_GetAccountInfo<Lambda>), static_cast<size_t>(DBBlock::SIZE)> CAsync_GetAccountInfo;
		}
	}
	~CAsync_GetAccountInfo() {}

	void ProcessingQuery()
	{
		CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
		int32_t ret = conn.RequestQuery(_query, _accountno);
		if(ret)
		{
			std::get<0>(_res) = static_cast<int64_t>(-ret);
			return;
		}

		MYSQL_ROW row;
		MYSQL_RES* res = conn.GetResult();
		if (res)
		{
			while ((row = mysql_fetch_row(res)))
			{
				_res = { std::stoll(row[0]), row[1], row[2], std::stoi(row[3]) };
			}
			conn.FreeResult();
		}
		return;
	}
	void ResponseProcess()
	{
		_processfunction(_res);
	}

private:
	int64_t _accountno;
	GetAccountInfoResType _res;
	Lambda _processfunction;
};


template <typename F>
concept CharacterInfoFunc = std::invocable<F, const GetCharacterInfoResType&>;
template <CharacterInfoFunc Lambda>
class CAsync_GetCharacterInfo : public IAsyncRequest
{
public:
	CAsync_GetCharacterInfo(EDBPoolUse who, uint64_t sessionId, int64_t accountNo, Lambda&& func)
		: IAsyncRequest(who, sessionId, QUERY_FORMATS[ASYNC_GET_CHARACTER_INFO]),
		_accountno(accountNo), _processfunction(std::forward<Lambda>(func)) {
		if constexpr (sizeof(CAsync_GetCharacterInfo<Lambda>) > DBBlock::SIZE)
		{
			CheckSize<sizeof(CAsync_GetCharacterInfo<Lambda>), static_cast<size_t>(DBBlock::SIZE)> CAsync_GetCharacterInfo;
		}
	}
	~CAsync_GetCharacterInfo() {}


	void ProcessingQuery()
	{
		CTlsMySqlConnector<LOCAL_DB>& conn = CTlsMySqlConnector<LOCAL_DB>::GetConnector();
		int32_t ret = conn.RequestQuery(_query, _accountno);
		if(ret)
		{
			std::get<0>(_res) = static_cast<int64_t>(-ret);
			return;
		}

		MYSQL_ROW row;
		MYSQL_RES* res = conn.GetResult();
		if (res)
		{
			while ((row = mysql_fetch_row(res)))
			{
				_res = { std::stoll(row[0]), std::stoi(row[1]), std::stof(row[2]), std::stof(row[3]), std::stoi(row[4]), std::stoi(row[5]), 
				std::stoi(row[6]), std::stoi(row[7]), std::stoi(row[8]), std::stoll(row[9]), std::stoi(row[10]), std::stoi(row[11])};
			}
			conn.FreeResult();
		}
		return;
	}
	void ResponseProcess()
	{
		_processfunction(_res);
	}
private:
	int64_t _accountno;
	GetCharacterInfoResType _res;
	Lambda _processfunction;
};


//-----------------------------------------------------
// WRITE
//-----------------------------------------------------

template<typename Lambda>
requires std::invocable<Lambda>
class CAsync_InsertGameLog : public IDBWriterRequest
{
public:
	CAsync_InsertGameLog():_query = ""
private:
	const wchar_t* _query;
};

#endif