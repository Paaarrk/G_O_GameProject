#include "Lobby.h"
#include "GameServer.h"
#include "Player.h"

#include <chrono>

#include "logclassV1.h"
using Log = Core::c_syslog;

CLobby::CLobby():useTimeout(true)
{
	_playerMap.reserve(GetMaxUsers());
	_accountIdToSessionIdMap.reserve(GetMaxUsers());
	_accountIdToPlayerMap.reserve(GetMaxUsers());
}
CLobby::~CLobby()
{

}

void CLobby::OnUpdate()
{
	if (useTimeout == true)
	{
		TimePoint curTime= SteadyClock::now();
		int64_t deltaTime;
		for (std::pair<const uint64_t, CPlayer*>& player : _playerMap)
		{
			deltaTime = GetDeltaTimeMs(curTime, player.second->GetLastRecvedTime());
			if (deltaTime > TIME_OUT_MS_LOBBY)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_SYSTEM, L"[AccountNo: %d] Not Logined, Timeout", player.second->GetAccountNo());
				player.second->PlayerDisconnected();
				CZone::Disconnect(player.first);
			}
		}
	}
}

void CLobby::OnEnter(uint64 sessionId, void* playerPtr, std::wstring* ip)
{

}
void CLobby::OnLeave(uint64 sessionId, bool bNeedPlayerDelete)
{
	
}
void CLobby::OnMessage(uint64 sessionId, const char* readPtr, int payloadlen)
{

}