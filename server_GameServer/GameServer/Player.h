#ifndef __PLAYER_H__
#define __PLAYER_H__
#include <stdint.h>
#include <string>
#include "Contents.h"
#include "TLSObjectPool_IntrusiveList.hpp"

//--------------------------------------------------------------
// |-----(1)-----|****(2)****|--(3)--|
// Login         User        Logout
// 로그인 서버               게임 서버
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
	PLAYER_LOGIN,				
	PLAYER_LOGIN_WAIT_LOAD,		// DB 로드 대기 상태 -> 언제든 잘려도 되는 상태 (아직 (1)번)

	// 여기부터 (2)번 상태
	PLAYER_IN_GAME,				
	PLAYER_IN_GAME_SELECT,		// 캐릭터 고르는 상태
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

	//------------------------------------------------------------
	// 기존 관리되는 로그아웃된 유저의 메모리를 그대로 가져옵니다
	//------------------------------------------------------------
	void LoadPlayer(CPlayer& origin) noexcept;


	void PlayerWaitLogin(unsigned long curtime, uint64_t sessionId, const std::wstring& wip) noexcept;
	void PlayerWaitRedisCheck(unsigned long curtime, uint64_t accountNo, char(&sessionKey)[SESSION_KEY_LEN], int32_t version) noexcept;
	void PlayerWaitDbCheck(unsigned long curtime, uint64_t sessionId) noexcept;
	void PlayerWaitLoad(unsigned long curtime) noexcept;
	void PlayerLogout(unsigned long curtime) noexcept;
	
	unsigned long SetLastRecvedTime(unsigned long time) { _recvedTime = time; }
	unsigned long GetLastRecvedTime() const noexcept { return _recvedTime; }
	int64_t GetAccountNo() const noexcept { return _accountNo; }
	int32_t GetPlayerStatus() const noexcept { return _playerStatus; }
	int32_t GetBefPlayerStatus() const noexcept { return _befPlayerStatus; }
	const wchar_t* GetPlayerIp() const noexcept { return _wip; }
	uint64_t GetSessionId() const noexcept { return _sessionId; }
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

	float _posX = 0.0f;
	float _posY = 0.0f;
	int32_t _rotate = 0;
	int32_t _tileX = 0;
	int32_t _tileY = 0;

	int32_t _hp = 0;
	uint64_t _exp = 0;
	int32_t _level = 0;
	int32_t _crystal = 0;
	wchar_t _nickname[GAME_NICKNAME_LEN] = {};

	wchar_t* _wip;
	char* _sessionKey;
	uint32_t _dbRequestCnt = 0;
	int32_t _befPlayerStatus = 0;
	
	inline static CTlsObjectPool<CPlayer, POOLKEY_PLAYER, TLS_OBJECTPOOL_USE_CALLONCE> s_playerPool;
};

#endif