#include "GameServer.h"
#include "_CrashDump.hpp"
#include "Player.h"
#include "Lobby.h"

Core::CCrashDump g_crashdump;

int main()
{
	CPlayer* p = CPlayer::Alloc();
	CGameServer g;
	CLobby lobby;
	char skey[64];
	int a;
	printf("%d", SizeOf());

	return 0;
}