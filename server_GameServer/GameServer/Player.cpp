#include "Player.h"
#include <malloc.h>

CPlayer::CPlayer() noexcept
{
	_wip = (wchar_t*)malloc(sizeof(wchar_t) * IPV4_LEN + sizeof(char) * SESSION_KEY_LEN);
	_sessionKey = reinterpret_cast<char*>(_wip) + sizeof(wchar_t) * IPV4_LEN;
#pragma warning (push)
#pragma warning (disable : 6387)
	memset(_wip, 0, sizeof(wchar_t) * IPV4_LEN + sizeof(char) * SESSION_KEY_LEN);
#pragma warning (pop)
}

CPlayer::~CPlayer()
{
	free(_wip);
}


//-----------------------------------------------------------------
// Player Status
//-----------------------------------------------------------------

void CPlayer::PlayerWaitLogin(uint64_t sessionId, const std::wstring& wip) noexcept
{
	_sessionId = sessionId;
	_accountNo = -1;
	_playerStatus = PLAYER_WAIT_LOGIN_PACKET;
	_recvedTime = SteadyClock::now();
	memcpy(_wip, wip.c_str(), sizeof(wchar_t) * IPV4_LEN);
}

void CPlayer::PlayerWaitRedisCheck(uint64_t accountNo, char(&sessionKey)[SESSION_KEY_LEN], int32_t version) noexcept
{
	_accountNo = accountNo;
	_gameVersion = version;
	_playerStatus = PLAYER_WAIT_REDIS_CHECKING;
	memcpy(_sessionKey, sessionKey, SESSION_KEY_LEN);
	_recvedTime = SteadyClock::now();
}

void CPlayer::PlayerWaitMasterAccept(uint64_t sequence, int32_t gameServerNo, int32_t masterNo) noexcept
{
	_sequence = sequence;
	_gameServerNo = gameServerNo;
	_chatServerNo = masterNo;
	_recvedTime = SteadyClock::now();
	_playerStatus = PLAYER_WAIT_MASTER_ACCEPT;
}

void CPlayer::PlayerLogin() noexcept
{
	_playerStatus = PLAYER_LOGIN;
}


void CPlayer::PlayerLogout() noexcept
{
	_playerStatus = PLAYER_LOGOUT;
}

