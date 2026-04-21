#ifndef __PLAYER_H__
#define __PLAYER_H__
#include <stdint.h>

//--------------------------------------
// todo db 관련 변수 추가 및 로직
//--------------------------------------
class CPlayer
{
public:

private:
	uint64_t _clientId = 0;
	uint64_t _sessionId = 0;
	int32_t _playerType = 0;

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

	wchar_t _ip[16] = {};
	wchar_t _nickname[20] = {};
};

#endif