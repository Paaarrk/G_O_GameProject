#pragma once
#include "TLSObjectPool_IntrusiveList.hpp"
inline static const wchar_t* TAG_DB = L"DB";

enum EMysqlConfig
{
	ID_LEN = 20,
	PW_LEN = 20,
};

enum EPhysicalInstance
{
	LOCAL_DB = 0,


	TOTAL_COUNT,
};

enum EDBPoolUse
{
	POOL_USE_LOBBY,


	POOL_USE_COUNT,
};
	
enum EDBPool
{
	ACCOUNTNO_NOT_FOUND = 0,
	DB_CONNECT_FAILED = -1,
	DB_QUERY_PROBLEM = -2,

	EXIT_SIGNAL = 0,
	REQUEST_QUERY = 1,

	IOCP_SETTING = 3,
	RUNNING = 4,
	CREATE = 16,
};

enum EDB_LOG_TYPE
{
	LOGIN_OUT = 1,
	PLAYER_CREATE_DELETE = 3,
	CRISTAL = 4,
	RECOVER_HP = 5,
	LOGIN_SERVER = 100,
};

enum EDB_LOG_CODE
{
	GAME_LOGIN = 11,
	GAME_LOGOUT = 12,

	GAME_PLAYER_DEATH = 31,
	GAME_PLAYER_CREATE = 32, 
	GAME_PLAYER_RESTART = 33,
	
	GAME_GET_CRISTAL = 41,

	GAME_RECOVER_HP = 51,

	LOGIN_LOGIN = 101,
};

constexpr const char* NAME_LOGIN = "Login";
constexpr const char* NAME_GAME = "Game";


struct DBBlock
{
	enum EBLOCK
	{ 
		SIZE = 256 
	};
	static char* Alloc() { return reinterpret_cast<char*>(blocks.Alloc()); }
	static int Free(char* block) 
	{
		int ret = blocks.Free(reinterpret_cast<DBBlock*>(block));
		return ret;
	}
	alignas(8) char mem[DBBlock::SIZE];
private:
	DBBlock() = delete;
	inline static CTlsObjectPool<DBBlock, 0x0000'0FEA, TLS_OBJECTPOOL_USE_RAW> blocks;
};
