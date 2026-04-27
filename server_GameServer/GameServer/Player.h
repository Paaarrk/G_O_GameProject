#ifndef __PLAYER_H__
#define __PLAYER_H__
#include <stdint.h>
#include <string>
#include "Contents.h"
#include "TLSObjectPool_IntrusiveList.hpp"

//--------------------------------------
// todo db 관련 변수 추가 및 로직
//--------------------------------------
enum EPlayerState
{
	PLAYER_WAIT_LOGIN_PACKET,	// 로그인 대기 상태
	PLAYER_WAIT_REDIS_CHECKING, // 레디스 체크
	PLAYER_WAIT_MASTER_ACCEPT,	// 마스터(채팅서버)의 허락 대기 상태
	PLAYER_LOGIN,				// 로그인 상태
	PLAYER_IN_GAME,				// 게임 안에 있는 상태
	PLAYER_LOGOUT,				// 로그아웃 상태, DB저장 대기 할 수 있음
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

	// 세션 키 확인 및 채팅, 게임서버 번호, 시퀀스 확인
	void PlayerWaitLogin(uint64_t sessionId, const std::wstring& wip) noexcept;
	void PlayerWaitRedisCheck(uint64_t accountNo, char(&sessionKey)[SESSION_KEY_LEN], int32_t version) noexcept;
	// 레디스에서 세션키 확인한 이후의 상태 
	// 채팅 서버에서 로그인 허용여부 확인 받아야 하는 상태
	void PlayerWaitMasterAccept(uint64_t sequence, int32_t gameServerNo, int32_t masterNo) noexcept;
	void PlayerLogin() noexcept;

	void PlayerLogout() noexcept;
	// void PlayerLogined();

	TimePoint GetLastRecvedTime() const noexcept { return _recvedTime; }
	int64_t GetAccountNo() const noexcept { return _accountNo; }
	int32_t GetPlayerStatus() const noexcept { return _playerStatus; }
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

	TimePoint _recvedTime = {};

	float _posX = 0.0f;
	float _posY = 0.0f;
	int32_t _rotate = 0;
	int32_t _tileX = 0;
	int32_t _tileY = 0;

	int32_t _hp = 0;
	int32_t _state = 0;
	uint64_t _exp = 0;
	int32_t _level = 0;
	int32_t _crystal = 0;
	wchar_t _nickname[GMAE_NICKNAME_LEN] = {};
	wchar_t* _wip;
	char* _sessionKey;
	int32_t _dbRequestCnt = 0;
	
	inline static CTlsObjectPool<CPlayer, POOLKEY_PLAYER, TLS_OBJECTPOOL_USE_CALLONCE> s_playerPool;
};

#endif