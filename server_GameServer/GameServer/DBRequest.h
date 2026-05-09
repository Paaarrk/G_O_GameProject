#ifndef __DB_REQUEST_H__
#define __DB_REQUEST_H__

#include "DBProtocol.h"
#include "DBConnector.hpp"
#include "DBThreadPool.h"
#include "DBWriter.h"
#include "Contents.h"
#include "staticdatas.h"
#include <vector>
#include <string>
#include <tuple>
#include <concepts>

#include "Player.h"

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

enum ERequestType
{
	ASYNC_GET_ACCOUNT_INFO,
	ASYNC_GET_CHARACTER_INFO,
	ASYNC_INSERT_CHARACTER,
	ASYNC_UPDATE_LOGOUT,
};

// [ASYNC_INSERT_CHARACTER]: accountno, playertype, game_static_hp, accountno, servername, playertype
inline static const wchar_t* QUERY_FORMATS[] = {
	L"SELECT `accountno`, `userId`, `usernick`, `status` FROM `accountdb`.`v_account` WHERE `accountno` = %lld",

	L"SELECT * FROM `gamedb`.`character` WHERE `accountno` = %lld",

	L"BEGIN;"
	L"INSERT INTO `gamedb`.`character` VALUES (%lld, %d, 0.0, 0.0, -1, -1, 0, 0, %d, 0, 0, 0);"
	L"INSERT INTO `logdb`.`gamelog_%04d_%02d` (`accountno`, `servername`, `type`, `code`, `param1`)"
	L"SELECT % lld, '%s', 3, 32, %d  FROM DUAL WHERE ROW_COUNT() > 0;"
	L"SELECT ROW_COUNT() ;"
	L"COMMIT;",

	L"BEGIN;"
	L"UPDATE `accountdb`.`status` SET `status` = 0 WHERE (`accountno` = %lld);"
	L"INSERT INTO `logdb`.`gamelog_%04d_%02d` (`accountno`, `servername`, `type`, `code`, `param1`, `param2`, `param3`, `param4`, `message`)"
	L"SELECT %lld, '%s', 1, 12, %d, %d, %d, %d, '%s' FROM DUAL WHERE ROW_COUNT() > 0; "
	L"SELECT ROW_COUNT() ;"
	L"COMMIT;"
};

//-----------------------------------------------------
// READ : Pool (Get Response)
//-----------------------------------------------------

template <typename F>
concept AccountInfoFunc = std::invocable<F, const GetAccountInfoResType&>;
template<AccountInfoFunc Lambda>
class CAsync_GetAccountInfo : public IAsyncRequest
{
public:
	CAsync_GetAccountInfo(EDBPoolUse who, uint64_t sessionId, int64_t accountNo, Lambda&& responseprocess)
		: IAsyncRequest(who, sessionId, QUERY_FORMATS[ASYNC_GET_ACCOUNT_INFO]),
		_accountno(accountNo), _processfunction(std::forward<Lambda>(responseprocess)) {
		if constexpr (sizeof(CAsync_GetAccountInfo<Lambda>) > DBBlock::SIZE)
		{
			CheckSize<sizeof(CAsync_GetAccountInfo<Lambda>), static_cast<size_t>(DBBlock::SIZE)> CAsync_GetAccountInfo;
		}
	}
	~CAsync_GetAccountInfo() {}

	void ProcessingQuery()
	{
		CTlsMySqlConnector<LOCAL_DB>& conn = GetConnector<LOCAL_DB>();
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
		CTlsMySqlConnector<LOCAL_DB>& conn = GetConnector<LOCAL_DB> ();
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
// WRITE : Pool (Get Response)
//-----------------------------------------------------

template <typename F>
concept InsertNewCharacterFunc = std::invocable<F, const InsertNewCharacterResType&>;
template <InsertNewCharacterFunc Lambda>
class CAsync_InsertNewCharacter : public IAsyncRequest
{
public:
	CAsync_InsertNewCharacter(EDBPoolUse who, uint64_t sessionId, CPlayer* playerptr, Lambda&& func, int64_t accountno, int32_t playertype, const wchar_t* servername, int32_t game_static_playerHp = CStaticDatas::Player_Hp())
		:IAsyncRequest(who, sessionId, QUERY_FORMATS[ASYNC_INSERT_CHARACTER]), _playerptr(playerptr), _processfunction(std::forward<Lambda>(func)),
		_accountno(accountno), _playertype(playertype), _servername(servername), _playerhp(game_static_playerHp)
	{
		if constexpr (sizeof(CAsync_InsertNewCharacter<Lambda>) > DBBlock::SIZE)
			CheckSize<sizeof(CAsync_InsertNewCharacter<Lambda>), static_cast<size_t>(DBBlock::SIZE)> CAsync_InsertNewCharacter;
	}
	~CAsync_InsertNewCharacter() override = default;

	void ProcessingQuery() override
	{
		tm tm;
		time_t mytime = time(NULL);
		localtime_s(&tm, &mytime);
		CTlsMySqlConnector<LOCAL_DB>& conn = GetConnector<LOCAL_DB>();
		// [ASYNC_INSERT_CHARACTER]: accountno, playertype, game_static_hp, accountno, servername, playertype
		int32_t ret = conn.RequestQuery(_query, _accountno, _playertype, _playerhp, tm.tm_year + 1900, tm.tm_mon + 1, _accountno, _servername, _playertype);

		// 여기서 뻑나면 카운트 관리의 문제
		_playerptr->DecreaseDBRequest();

		int status = 0;
		int affectrowcount = 0;
		while (status == 0)
		{
			if (conn.GetErrno())
			{
				// ret2 , 연결 끊김 -> 그냥 끊으면됨 (저장 됫다면 재접속시 정보O, 아니면 그때 다시보내면됨)
				_res = { _accountno, false };
				conn.LogSQLError();
				conn.RequestQuery(L"ROLLBACK");
				return;
			}

			MYSQL_RES* res = conn.GetResult();
			if (res)
			{
				MYSQL_ROW row = mysql_fetch_row(res);
				affectrowcount = std::stoi(row[0]);
				conn.FreeResult();
			}
			status = conn.MySqlNextResult();
		}

		if (status > 0)	// 커밋이 실패함
		{
			if (conn.GetErrno())
			{
				// ret2, 연결 끊김 ->  그냥 끊으면됨 (저장 됫다면 재접속시 정보O, 아니면 그때 다시보내면됨)
				conn.LogSQLError();
			}
			_res = { _accountno, false };
			conn.RequestQuery(L"ROLLBACK");
			return;
		}
		else if (affectrowcount == 0)	// 커밋은 성공했는데 쿼리 결과에 문제가 있음.
		{
			wchar_t query[CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE];
			swprintf_s(query, CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE, this->_query, _accountno, _playertype, _playerhp, tm.tm_year + 1900, tm.tm_mon + 1, _accountno, _servername, _playertype);
			Log::logging().Log(TAG_DB, Log::en_ERROR, L"affectrowcount = 0... check query %s", query);
			_res = { _accountno, false };
			return;
		}

		_res = { _accountno, true };
	}
	void ResponseProcess() override
	{
		_processfunction(_res);
	}
private:
	CPlayer* _playerptr;
	int64_t _accountno;
	int32_t _playertype;
	const wchar_t* _servername;
	int32_t _playerhp;

	InsertNewCharacterResType _res;
	Lambda _processfunction;
};


template <typename F>
concept UpdateLogoutFunc = std::invocable<F, const UpdateLogoutResType&>;
template <UpdateLogoutFunc Lambda>
class CAsync_UpdateLogout : public IAsyncRequest
{
public:
	CAsync_UpdateLogout(EDBPoolUse who, uint64_t sessionId, CPlayer* player, Lambda&& responsefunc, int64_t accountno,
		int32_t tilex, int32_t tiley, int32_t cristal, int32_t hp, const wchar_t* message) : IAsyncRequest(who, sessionId, QUERY_FORMATS[ASYNC_UPDATE_LOGOUT]),
		_player(player), _processfunction(std::forward<Lambda>(responsefunc)), _accountno(accountno), _tilex(tilex), _tiley(tiley),
		_cristal(cristal), _hp(hp), _message(message)
	{
		if constexpr (sizeof(CAsync_UpdateLogout<Lambda>) > DBBlock::SIZE)
			CheckSize<sizeof(CAsync_UpdateLogout), static_cast<size_t>(DBBlock::SIZE)> CAysnc_UpdateLogout;
	}
	~CAsync_UpdateLogout() override = default;
	void ProcessingQuery() override
	{
		tm tm;
		time_t mytime = time(NULL);
		localtime_s(&tm, &mytime);
		CTlsMySqlConnector<LOCAL_DB>& conn = GetConnector<LOCAL_DB>();
		// [ASYNC_UPDATE_LOGOUT]: accountno, year, mon, accountno, servername, tilex, tiley, cristal, hp
		int32_t ret = conn.RequestQuery(_query, _accountno, tm.tm_year + 1900, tm.tm_mon + 1,
			_accountno, SERVER_GAME, _tilex, _tiley, _cristal, _hp, _message);

		_player->DecreaseDBRequest();

		int status = 0;
		int affectrowcount = 0;
		while (status == 0)
		{
			if (conn.GetErrno())
			{
				// ret2 , 연결 끊김 -> 그냥 끊으면됨 (저장 됫다면 재접속시 정보O, 아니면 그때 다시보내면됨)
				_res = { _accountno, false };
				conn.LogSQLError();
				conn.RequestQuery(L"ROLLBACK");
				return;
			}

			MYSQL_RES* res = conn.GetResult();
			if (res)
			{
				MYSQL_ROW row = mysql_fetch_row(res);
				affectrowcount = std::stoi(row[0]);
				conn.FreeResult();
			}
			status = conn.MySqlNextResult();
		}

		if (status > 0)	// 커밋이 실패함
		{
			if (conn.GetErrno())
			{
				// ret2, 연결 끊김 ->  그냥 끊으면됨 (저장 됫다면 재접속시 정보O, 아니면 그때 다시보내면됨)
				conn.LogSQLError();
			}
			_res = { _accountno, false };
			conn.RequestQuery(L"ROLLBACK");
			return;
		}
		else if (affectrowcount == 0)	// 커밋은 성공했는데 쿼리 결과에 문제가 있음.
		{
			wchar_t query[CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE];
			swprintf_s(query, CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE, this->_query, _accountno, tm.tm_year + 1900, tm.tm_mon + 1,
				_accountno, L"Game", _tilex, _tiley, _cristal, _hp, _message);
			Log::logging().Log(TAG_DB, Log::en_ERROR, L"affectrowcount = 0... check query %s", query);
			_res = { _accountno, false };
			return;
		}

		_res = { _accountno, true };
	}
	void ResponseProcess() override
	{
		_processfunction(_res);
	}
private:
	CPlayer* _player;
	int64_t _accountno;
	int32_t _tilex;
	int32_t _tiley;
	int32_t _cristal;
	int32_t _hp;
	const wchar_t* _message;
	UpdateLogoutResType _res;
	Lambda _processfunction;
};



//-----------------------------------------------------
// WRITE Thread
//-----------------------------------------------------


// No Response 전용으로만..
// _process의 (int ret)의 경우 0: 정상 / 1: DB연결 끊김 / 2: 쿼리 잘못 / 3: 영향받은row가 0개
template <typename F, typename... Args>
requires std::invocable<F, int>
class CAsync_WriteRequest : public IDBWriterRequest
{
public:
	CAsync_WriteRequest(const wchar_t* query, F&& process, Args&&... args)
	:_query(query), _process(std::forward<F>(process)), _args(std::forward<Args>(args)...)
	{

	}
	~CAsync_WriteRequest() override = default;

	// 해당 작업이 완료되면 작업객체는 소멸됩니다
	void WriteProcess() override
	{
		CTlsMySqlConnector<LOCAL_DB>& conn = GetConnector<LOCAL_DB>();
		int ret = std::apply([&conn, this](auto&&... unpackArgs) {
			return conn.RequestQuery(this->_query, ToRaw(std::forward<decltype(unpackArgs)>(unpackArgs))...);
		}, _args);

		int status = 0;
		int affectrowcount = 0;
		while (status == 0)
		{
			if (conn.GetErrno())
			{
				ret = 2;
				conn.LogSQLError();
			}

			MYSQL_RES* res = conn.GetResult();
			if(res)
			{
				MYSQL_ROW row = mysql_fetch_row(res);
				affectrowcount = std::stoi(row[0]);
				conn.FreeResult();
			}
			status = conn.MySqlNextResult();
		}

		if (status > 0)	// 커밋이 실패함
		{
			if (conn.GetErrno())
			{
				ret = 2;
				conn.LogSQLError();
			}
			conn.RequestQuery(L"ROLLBACK");
		}
		else if (affectrowcount == 0)	// 커밋은 성공했는데 쿼리 결과에 문제가 있음.
		{
			ret = 3;
			wchar_t query[CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE];
			std::apply([&query, this](auto&&... unpackArgs) {
				swprintf_s(query, CTlsMySqlConnector<LOCAL_DB>::QUERY_SIZE, this->_query, ToRaw(std::forward<decltype(unpackArgs)>(unpackArgs))...);
			}, _args);
			Log::logging().Log(TAG_DB, Log::en_ERROR, L"affectrowcount = 0... check query %s", query);
		}


		_process(ret);	// 후속처리
	}
private:
	const wchar_t* _query;
	std::tuple<std::decay_t<Args>...> _args;
	F _process;
};

#endif