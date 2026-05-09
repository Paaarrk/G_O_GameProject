#include "Player.h"
#include <malloc.h>

CPlayer::CPlayer() noexcept :_pos(0.0f, 0.0f, (uint16_t)0)
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

bool CPlayer::LoadPlayer(CPlayer& origin) noexcept
{
	if (origin._befPlayerStatus < PLAYER_IN_GAME) 
		return false;

	_playerStatus = origin._befPlayerStatus;
	_playerType = origin._playerType;

	_pos = origin._pos;
	_tileX = static_cast<int32_t>(_pos.x * static_cast<float>(EMap::TILE_X / EMap::X));
	_tileY = static_cast<int32_t>(_pos.y * static_cast<float>(EMap::TILE_Y / EMap::Y));
	_hp = origin._hp;
	_exp = origin._exp;
	_level = origin._level;
	_cristal = origin._cristal;
	// wcscpy_s(_nickname, origin._nickname);. 지금은 db에서 읽지 않고 클라가 들고옴
	return true;
}

void CPlayer::LoadPlayer(const GetCharacterInfoResType& dbload) noexcept
{
	const auto& [accountno, charactertype, posx, posy, tilex, tiley, rotation, cristal, hp, exp, level, die] = dbload;
	_playerType = charactertype;
	_pos.SetPos(posy, posx, rotation);
	_tileX = tilex;
	_tileY = tiley;
	_cristal = cristal;
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

void CPlayer::PlayerWaitGameSelect(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_LOGIN_GAME_SELECT;
	_recvedTime = curtime;
}

void CPlayer::PlayerWaitSelectDBSave(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_LOGIN_WAIT_SELECT_DB_SAVE;
	_recvedTime = curtime;
}

void CPlayer::PlayerWaitGoInGame(unsigned long curtime) noexcept
{
	_playerStatus = PLAYER_LOGIN_WAIT_GO_GAME;
	_recvedTime = curtime;
}


void CPlayer::PlayerLogout(unsigned long curtime) noexcept
{
	_befPlayerStatus = _playerStatus;
	_playerStatus = PLAYER_LOGOUT;
	_recvedTime = curtime;
}

void CPlayer::SetPlayerFirstCreate(int32_t playerType, int32_t hp)
{
	_playerType = playerType;
	_hp = hp; _tileX = -1; _tileY = -1;
	_exp = 0; _level = 0; _cristal = 0;
}

