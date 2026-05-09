#include "GameServer.h"
#include "_CrashDump.hpp"
#include "Player.h"
#include "Lobby.h"
#include "DBConnector.hpp"
#include "DBRequest.h"
#include "staticdatas.h"
#include "NetProcess.h"

#include <locale.h>
#include <timeapi.h>
#pragma comment(lib, "winmm")

Core::CCrashDump g_crashdump;
using Log = Core::c_syslog;
CGameServer g_game;
int main()
{
	timeBeginPeriod(1);
	setlocale(LC_ALL, "korean");

	// 옵션 로드
	CGameServer::stGameServerOpt opt;
	if (opt.LoadOption() == false)
	{
		Log::logging().Log(TAG_LOAD, Log::en_SYSTEM, L"Config Load failed");
		return 0;
	}

	// DB 초기화
	mysql_library_init(0, NULL, NULL);
	GetConnector<LOCAL_DB>().SetConnector(opt.mysqlIp, opt.mysql_id, opt.mysql_pw, nullptr, opt.mysqlPort);

	// 게임 정적 데이터 초기화
	if (CStaticDatas::Init() == false)
	{
		Log::logging().Log(TAG_CONTENTS, Log::en_SYSTEM, L"Static Data Load failed");
		return 0;
	}

	// 서버 이닛
	bool ret = g_game.Init(reinterpret_cast<const Net::CZoneServer::stServerOpt*>(&opt));
	if (ret == false) return 0;
	
	ret = g_game.Start();
	if (ret == false) return 0;

	while (1) { ; }

	g_game.Stop(1);
	Net::CNetProcess::GetProcessTimer().ExitTimer();
	return 0;
}