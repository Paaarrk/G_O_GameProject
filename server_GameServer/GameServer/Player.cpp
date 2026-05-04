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

void CPlayer::LoadPlayer(CPlayer& origin) noexcept
{
	_playerStatus = origin._befPlayerStatus;
	_playerType = origin._playerType;

	// TODO
	// 멀티스레드로 접근하는 일부 자원(db참조카운트 등)은
	// 반드시 move형태로 옮겨져야한다.
	_posX = origin._posX;
	_posY = origin._posY;
	_rotate = origin._rotate;
	_tileX = static_cast<int32_t>(_posX) * 2;
	_tileY = static_cast<int32_t>(_posY) * 2;
	_hp = origin._hp;
	_exp = origin._exp;
	_level = origin._level;
	_crystal = origin._crystal;
	wcscpy_s(_nickname, origin._nickname);
}

void CPlayer::LoadPlayer(const GetCharacterInfoResType& dbload) noexcept
{
	const auto& [accountno, charactertype, posx, posy, tilex, tiley, rotation, cristal, hp, exp, level, die] = dbload;
	_playerType = charactertype;
	_posX = posx;
	_posY = posy;
	_tileX = tilex;
	_tileY = tiley;
	_rotate = rotation;
	_crystal = cristal;
	_hp = hp;
	_exp = exp;
	_level = level;
	if (die) _playerStatus = PLAYER_IN_GAME_DEAD;
	else _playerStatus = PLAYER_IN_GAME_ALIVE;
}


//-----------------------------------------------------------------
// Player Status
//-----------------------------------------------------------------

void CPlayer::PlayerWaitLogin(unsigned long curtime, uint64_t sessionId, const std::wstring& wip) noexcept
{
	_sessionId = sessionId;
	_accountNo = -1;
	_playerStatus = PLAYER_WAIT_LOGIN_PACKET;
	memcpy(_wip, wip.c_str(), sizeof(wchar_t) * IPV4_LEN);
	_recvedTime = curtime;
}

void CPlayer::PlayerWaitRedisCheck(unsigned long curtime, uint64_t accountNo, char(&sessionKey)[SESSION_KEY_LEN], int32_t version) noexcept
{
	_accountNo = accountNo;
	_gameVersion = version;
	_playerStatus = PLAYER_WAIT_REDIS_CHECKING;
	memcpy(_sessionKey, sessionKey, SESSION_KEY_LEN);
	_recvedTime = curtime;
}

void CPlayer::PlayerWaitDbCheck(unsigned long curtime, uint64_t sessionId) noexcept
{
	_playerStatus = PLAYER_WAIT_DB_CHECKING;
	_recvedTime = curtime;
}

void CPlayer::PlayerWaitLoad(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_LOGIN_WAIT_LOAD;
	_recvedTime = curtime;
}

void CPlayer::PlayerInGameSelect(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_IN_GAME_SELECT;
	_recvedTime = curtime;
}


void CPlayer::PlayerLogout(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_LOGOUT;
	_recvedTime = curtime;
}

