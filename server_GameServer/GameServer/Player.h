#ifndef __PLAYER_H__
#define __PLAYER_H__
#include <stdint.h>
#include <string>
#include <atomic>
#include "Contents.h"
#include "CustomPos.h"
#include "TLSObjectPool_IntrusiveList.hpp"

//--------------------------------------------------------------
// |-----(1)-----|--(2-1)--|****(2-2)****|--(3)--|
// Login         User       game         Logout
// 로그인 서버   게임 서버
//----------------------------------------------------------
enum EPlayerState
{
	// 유저가 오면
	// 1) 로그인 패킷 기반으로 레디스 세션키 체크
	// 2) 로그인 도장 찍힌지 확인 대기 상태 
	// 3) (1)번의 상태이므로 언제든 추방 가능 (로그인 서버가 kick요청 보냈을 때 Logout으로 바꾸면 되서)
	PLAYER_WAIT,				
	PLAYER_WAIT_LOGIN_PACKET,	// 로그인 대기 상태
	PLAYER_WAIT_REDIS_CHECKING, // 레디스 체크 (세션 키 체크)
	PLAYER_WAIT_DB_CHECKING,	// 로그인 도장 찍힌지 확인 대기 상태

	// 여기부터 (2-1)번 상태
	PLAYER_LOGIN,				
	PLAYER_LOGIN_WAIT_LOAD,		// DB 로드 대기 상태 -> 언제든 잘려도 되는 상태 (아직 (1)번)
	PLAYER_LOGIN_GAME_SELECT,	// 캐릭터 고르는 상태 (메모리 안남음) -> DB패킷을 보냈다면 잘리면 안됨
	PLAYER_LOGIN_WAIT_SELECT_DB_SAVE,	// 새로 만든 캐릭터 저장하는 상태
	PLAYER_LOGIN_WAIT_GO_GAME,	// 캐릭터 저장 시 DB req가 0이 되어야 이동 할 수 있어서 대기상태

	// 여기부터 (2-2)번 상태
	PLAYER_IN_GAME,				
	PLAYER_IN_GAME_ALIVE,		// 게임 안에 살아있는 상태
	PLAYER_IN_GAME_DEAD,		// 게임 안에 죽은 상태
	PLAYER_IN_GAME_SIT,			// 게임 안에 앉은 상태
	
	PLAYER_LOGOUT,				// 로그아웃 상태, DB저장 대기 할 수 있음, 완료 시 킥 가능
};
class CPlayer
{
	template <typename T, int, int>
	friend class CTlsObjectPool;
public:
	static CPlayer* Alloc()
	{
		CPlayer* ret = s_playerPool.Alloc();
		if (ret == nullptr)
			return nullptr;
		return ret;
	}

	//------------------------------------------------------------
	// 0: 정상
	// 1: 언더플로
	// 2: 오버플로
	// 3: 널ptr
	// 4: 이중 해제
	//------------------------------------------------------------
	static int Free(CPlayer* pPlayer)
	{
		return s_playerPool.Free(pPlayer);
	}

	// 기존 관리되는 로그아웃된 유저의 메모리를 그대로 가져옵니다
	// 가져와도 유효한 상태면 true, 아니면 false
	bool LoadPlayer(CPlayer& origin) noexcept;
	void LoadPlayer(const GetCharacterInfoResType& dbload) noexcept;
	void MakeMemoryStatus(unsigned long curtime) noexcept {
		_befPlayerStatus = _playerStatus; 
		_playerStatus = PLAYER_LOGOUT;
		_recvedTime = curtime;
	}

	void PlayerWaitLogin(unsigned long curtime, uint64_t sessionId, const std::wstring& wip) noexcept;
	void PlayerWaitRedisCheck(unsigned long curtime, uint64_t accountNo, char(&sessionKey)[SESSION_KEY_LEN], int32_t version) noexcept;
	void PlayerWaitDbCheck(unsigned long curtime, uint64_t sessionId) noexcept;
	void PlayerWaitLoad(unsigned long curtime) noexcept;
	void PlayerWaitGameSelect(unsigned long curtime) noexcept;
	void PlayerWaitSelectDBSave(unsigned long curtime) noexcept;
	void PlayerWaitGoInGame(unsigned long curtime) noexcept;
	
	void PlayerLogout(unsigned long curtime) noexcept;

	void SetPlayerName(const stName& name)
	{
		MultiByteToWideChar(CP_UTF8, 0, name.name, -1, _nickname, GAME_NICKNAME_LEN);
	}
	void SetPlayerFirstCreate(int32_t playerType, int32_t hp);

	unsigned long SetLastRecvedTime(unsigned long time) { _recvedTime = time; }
	unsigned long GetLastRecvedTime() const noexcept { return _recvedTime; }
	int64_t GetAccountNo() const noexcept { return _accountNo; }
	int32_t GetPlayerStatus() const noexcept { return _playerStatus; }
	int32_t GetBefPlayerStatus() const noexcept { return _befPlayerStatus; }
	const wchar_t* GetPlayerIp() const noexcept { return _wip; }
	uint64_t GetSessionId() const noexcept { return _sessionId; }
	int32_t GetTileX() const noexcept { return _tileX; }
	int32_t GetTileY() const noexcept { return _tileY; }
	int32_t GetCristal() const noexcept { return _cristal; }
	int32_t GetHp() const noexcept { return _hp; }

	//zone에서만 호출 가능
	void IncreaseDBRequest() noexcept { ++_dbRequestCnt; }
	//db컨텍스트 에서만 호출 가능
	void DecreaseDBRequest() noexcept { --_dbRequestCnt; }
	//zone에서만 호출 가능
	bool DBRequestFin() noexcept { return (_dbRequestCnt.load(std::memory_order_relaxed) == 0); }
private:
	CPlayer() noexcept;
	~CPlayer() noexcept;


	uint64_t _sessionId = 0;
	int64_t _accountNo = 0;
	int32_t _gameVersion = 0;
	int32_t _gameServerNo = 0;
	int32_t _chatServerNo = 0;
	uint64_t _sequence = 0;
	int32_t _playerStatus = 0;
	int32_t _playerType = 0;

	unsigned long _recvedTime = 0;

	CustomPos _pos;
	int32_t _tileX = -1;
	int32_t _tileY = -1;

	int32_t _hp = 0;
	uint64_t _exp = 0;
	int32_t _level = 0;
	int32_t _cristal = 0;
	wchar_t _nickname[GAME_NICKNAME_LEN] = {};

	wchar_t* _wip;
	char* _sessionKey;
	std::atomic<uint32_t> _dbRequestCnt = 0;
	int32_t _befPlayerStatus = 0;
	
	inline static CTlsObjectPool<CPlayer, POOLKEY_PLAYER, TLS_OBJECTPOOL_USE_CALLONCE> s_playerPool;
};

#endif