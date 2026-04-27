#ifndef __GAME_SERVER_H__
#define __GAME_SERVER_H__
#include "ZoneServer.h"
#include "Contents.h"
#include "RedisConnector.h"

class CGameServer : Net::CZoneServer
{
public:
	struct stGameServerOpt : public stServerOpt
	{
		int32_t lobbyMaxUser;
		int32_t gameMaxUser;
		int32_t lobbyMinimumTick;
		int32_t gameMinimumTick;
		int32_t maxZoneCnt;

		char	redisIp[IPV4_LEN];
		int16_t authRedisPort;

		bool LoadOption(const char* path = CONFIG_FILE_PATH);
	};

	//-----------------------------------------------------
	// Event Functions
	//-----------------------------------------------------
	virtual bool OnInit(const stServerOpt* pOpt);
	virtual bool OnAccept(uint64_t sessionId, in_addr ip, wchar_t* wip);
	virtual bool OnConnectionRequest(in_addr ip);

	// NetInterface

	virtual void OnRelease(uint64_t sessionId);
	virtual void OnMessage(uint64_t sessionId, Net::CPacket* pPacket, int len);

	// IOCP차원

	virtual void OnWorkerStart();
	virtual void OnWorkerEnd();
	virtual void OnUserEvent(Net::CPacket* pPacket);

	// 중지와 종료

	virtual void OnStop();
	virtual void OnExit();

	// 생성자
	CGameServer();
	virtual ~CGameServer();

	// 레디스 커넥터
	static CRedisConnector& GetRedisConnector() { return s_conn_auth; }
private:
	uint64_t _lobbyId = 0;

	static CRedisConnector s_conn_auth;
};

#endif