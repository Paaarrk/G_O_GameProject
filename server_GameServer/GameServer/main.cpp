#include "GameServer.h"
#include "_CrashDump.hpp"
#include "Player.h"

Core::CCrashDump g_crashdump;

int main()
{
	CPlayer* p = CPlayer::Alloc();
	CGameServer g;

	return 0;
}