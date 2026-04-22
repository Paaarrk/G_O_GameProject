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

void CPlayer::PlayerConnected(uint64_t sessionId, const wchar_t* wip) noexcept
{
	_sessionId = sessionId;
	_accountNo = -1;
	_playerStatus = PLAYER_CONNECTED;
	_recvedTime = SteadyClock::now();
	memcpy(_wip, wip, sizeof(wchar_t) * IPV4_LEN);
}

void CPlayer::PlayerDisconnected()
{
	_playerStatus = PLAYER_DISCONNECTED;
}

