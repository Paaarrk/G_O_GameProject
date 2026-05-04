#include "GameServer.h"
#include "Contents.h"
#include "21_TextParser.h"

#include "DBConnector.hpp"

#include "logclassV1.h"
using Log = Core::c_syslog;

CClientToLoginServer CGameServer::s_toLoginServerClient;
CRedisConnector CGameServer::s_conn_auth;


bool CGameServer::stGameServerOpt::LoadOption(const char* path)
{
	CParser config(path);
	try
	{
		config.LoadFile();

		int bUseEncode;
		config.GetValue("GAME_SERVER", "bUseEncode", &bUseEncode);
		this->bUseEncode = (bool)bUseEncode;

		int bUseSO_SNDBUF;
		config.GetValue("GAME_SERVER", "bUseSO_SNDBUF", &bUseSO_SNDBUF);
		this->bUseSO_SNDBUF = (bool)bUseSO_SNDBUF;

		int bUseTCP_NODELAY;
		config.GetValue("GAME_SERVER", "bUseTCP_NODELAY", &bUseTCP_NODELAY);
		this->bUseTCP_NODELAY = (bool)bUseTCP_NODELAY;

		int iMaxConcurrentUsers;
		config.GetValue("GAME_SERVER", "iMaxConcurrentUsers", &iMaxConcurrentUsers);
		this->iMaxConcurrentUsers = iMaxConcurrentUsers;

		int iWorkerThreadCreateCnt;
		config.GetValue("GAME_SERVER", "iWorkerThreadCreateCnt", &iWorkerThreadCreateCnt);
		this->iWorkerThreadCreateCnt = iWorkerThreadCreateCnt;

		int iWorkerThreadRunCnt;
		config.GetValue("GAME_SERVER", "iWorkerThreadRunCnt", &iWorkerThreadRunCnt);
		this->iWorkerThreadRunCnt = iWorkerThreadRunCnt;

		char openIP[16];
		config.GetValue("GAME_SERVER", "openIP", openIP);
		MultiByteToWideChar(CP_ACP, 0, openIP, 16, this->openIP, 16);

		int port;
		config.GetValue("GAME_SERVER", "port", &port);
		this->port = (unsigned short)port;

		int serverCode;
		config.GetValue("GAME_SERVER", "serverCode", &serverCode);
		server_code = (uint8)serverCode;

		int staticKey;
		config.GetValue("GAME_SERVER", "staticKey", &staticKey);
		this->static_key = (uint8)staticKey;

		// Game

		int lobbyMaxUser;
		config.GetValue("GAME_SERVER", "lobbyMaxUsers", &lobbyMaxUser);
		this->lobbyMaxUser = lobbyMaxUser;

		int gameMaxUsers;
		config.GetValue("GAME_SERVER", "gameMaxUsers", &gameMaxUsers);
		this->gameMaxUser = gameMaxUsers;

		int lobbyMinimumTick;
		config.GetValue("GAME_SERVER", "lobbyMinimumTick", &lobbyMinimumTick);
		this->lobbyMinimumTick = lobbyMinimumTick;

		int gameMinimumTick;
		config.GetValue("GAME_SERVER", "gameMinimumTick", &gameMinimumTick);
		this->gameMinimumTick = gameMinimumTick;

		int maxZoneCnt;
		config.GetValue("GAME_SERVER", "maxZoneCnt", &maxZoneCnt);
		this->maxZoneCnt = maxZoneCnt;
	}
	catch (std::invalid_argument& e)
	{
		wchar_t buffer[256];
		MultiByteToWideChar(CP_ACP, 0, e.what(), -1, buffer, 256);
		Log::logging().Log(TAG_CONTENTS, Log::en_ERROR,
			L"%s", buffer);
		return false;
	}
	return true;
}

//-----------------------------------------------------
// Event Functions
//-----------------------------------------------------
bool CGameServer::OnInit(const Net::CZoneServer::stServerOpt* pOpt)
{
	stGameServerOpt& opt = *(stGameServerOpt*)pOpt;
	opt.LoadOption();

	// Redis Connector 초기화
	CGameServer::s_conn_auth.Init(opt.redisIp, opt.authRedisPort, 3);
	Net::CClient::stClientOpt clientopt;
	clientopt.bUseBind = false;
	clientopt.bUseEncode = false;
	clientopt.bUseSO_SNDBUF = true;
	clientopt.bUseTCP_NODELAY = false;
	clientopt.client_code = opt.loginCode;
	clientopt.iWorkerThreadCreateCnt = 1;
	clientopt.iWorkerThreadRunCnt = 1;
	memcpy(clientopt.targetIP, opt.loginIp, IPV4_LEN);
	clientopt.targetPort = opt.loginPort;

	// DB 초기화
	GetConnector<LOCAL_DB>().SetConnector(opt.mysqlIp, opt.mysql_id, opt.mysql_pw, nullptr, opt.mysqlPort);
	

	if (CGameServer::s_toLoginServerClient.Init(&clientopt) == false)
		return false;
	

	return true;
}
bool CGameServer::OnAccept(uint64_t sessionId, in_addr ip, wchar_t* wip)
{

	return true;
}
bool CGameServer::OnConnectionRequest(in_addr ip)
{

	return true;
}

// NetInterface

void CGameServer::OnRelease(uint64_t sessionId)
{

}
void CGameServer::OnMessage(uint64_t sessionId, Net::CPacket* pPacket, int len)
{

}

// IOCP차원

void CGameServer::OnWorkerStart()
{

}
void CGameServer::OnWorkerEnd()
{

}
void CGameServer::OnUserEvent(Net::CPacket* pPacket)
{

}

// 중지와 종료

void CGameServer::OnStop()
{

}
void CGameServer::OnExit()
{

}

// 생성자
CGameServer::CGameServer()
{

}
// 소멸자
CGameServer::~CGameServer()
{

}