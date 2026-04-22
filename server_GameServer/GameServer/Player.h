#ifndef __PLAYER_H__
#define __PLAYER_H__
#include <stdint.h>
#include "Contents.h"
#include "TLSObjectPool_IntrusiveList.hpp"

//--------------------------------------
// todo db 관련 변수 추가 및 로직
//--------------------------------------
enum EPlayerState
{
	PLAYER_CONNECTED,
	PLAYER_IN_GAME,
	PLAYER_DISCONNECTED,
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

	void PlayerConnected(uint64_t sessionId, const wchar_t* wip) noexcept;
	void PlayerDisconnected() noexcept;
	// void PlayerLogined();

	TimePoint GetLastRecvedTime() const noexcept { return _recvedTime; }
	int64_t GetAccountNo() const noexcept { return _accountNo; }
private:
	CPlayer() noexcept;
	~CPlayer() noexcept;


	uint64_t _sessionId = 0;
	int64_t _accountNo = 0;
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
	
	inline static CTlsObjectPool<CPlayer, POOLKEY_PLAYER, TLS_OBJECTPOOL_USE_CALLONCE> s_playerPool;
};

#endif