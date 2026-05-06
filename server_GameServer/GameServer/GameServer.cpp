#include "GameServer.h"
#include "Contents.h"

#include "ZoneType.h"
#include "Lobby.h"
#include "GameZone.h"

#include "DBConnector.hpp"

#include "21_TextParser.h"
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
		int lobbyMinimumTick;
		config.GetValue("GAME_SERVER", "lobbyMinimumTick", &lobbyMinimumTick);
		this->lobbyMinimumTick = lobbyMinimumTick;

		int gameMaxUsers;
		config.GetValue("GAME_SERVER", "gameMaxUsers", &gameMaxUsers);
		this->gameMaxUser = gameMaxUsers;
		int gameMinimumTick;
		config.GetValue("GAME_SERVER", "gameMinimumTick", &gameMinimumTick);
		this->gameMinimumTick = gameMinimumTick;

		int selectMaxUser;
		config.GetValue("GAME_SERVER", "selectMaxUsers", &selectMaxUser);
		this->selectMaxUser = selectMaxUser;
		int selectMinimumTick;
		config.GetValue("GAME_SERVER", "selectMinimumTick", &selectMinimumTick);
		this->selectMinimumTick = selectMinimumTick;

		int maxZoneCnt;
		config.GetValue("GAME_SERVER", "maxZoneCnt", &maxZoneCnt);
		this->maxZoneCnt = maxZoneCnt;

		// Redis
		config.GetValue("REDIS_SESSIONKEY", "redisIP", this->redisIp);
		int redisPort;
		config.GetValue("REDIS_SESSIONKEY", "redisPort", &redisPort);
		this->authRedisPort = static_cast<int16_t>(redisPort);

		// LOCAL_DB
		config.GetValue("DB_MYSQL_LOCAL_DB", "mysqlIP", this->mysqlIp);
		int mysqlPort;
		config.GetValue("DB_MYSQL_LOCAL_DB", "mysqlPort", &mysqlPort);
		this->mysqlPort = static_cast<int16_t>(mysqlPort);
		config.GetValue("DB_MYSQL_LOCAL_DB", "mysqlId", this->mysql_id);
		config.GetValue("DB_MYSQL_LOCAL_DB", "mysqlPw", this->mysql_pw);

		// To LoginServer
		int code, key;
		config.GetValue("LOGIN_SERVER", "serverCode", &code);
		config.GetValue("LOGIN_SERVER", "staticKey", &key);
		this->loginCode = static_cast<uint8_t>(code);
		this->loginKey = static_cast<uint8_t>(key);
		char aloginip[IPV4_LEN];
		config.GetValue("LOGIN_SERVER", "internalOpenIP", aloginip);
		MultiByteToWideChar(CP_ACP, 0, aloginip, -1, this->loginIp, IPV4_LEN);
		int loginport;
		config.GetValue("LOGIN_SERVER", "internalPort", &loginport);
		this->loginPort = static_cast<int16_t>(loginport);

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

	// Redis Connector 초기화
	CGameServer::s_conn_auth.Init(opt.redisIp, opt.authRedisPort, 3);

	// login서버 코넥트 초기화
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
	
	// 클라이언트 이닛
	if (CGameServer::s_toLoginServerClient.Init(&clientopt) == false)
	{
		Log::logging().Log(TAG_TO_LOGIN, Log::en_ERROR, L"Init failed");
		return false;
	}
	
	// 존 타입 등록
	GetZoneManager().RegisterZoneType(
		Net::MakeZoneType<CLobby>(ZONE_TYPE_LOBBY, opt.lobbyMinimumTick, opt.lobbyMaxUser, false, 0));
	GetZoneManager().RegisterZoneType(
		Net::MakeZoneType<CGameZone>(ZONE_TYPE_INGAME, opt.gameMinimumTick, opt.gameMaxUser, true, ZONE_WEIGHT_INGAME));
	
	GetZoneManager().Init(opt.maxZoneCnt, 1);
	// 존 생성
	_lobbyId = GetZoneManager().CreateZone(ZONE_TYPE_LOBBY);
	_lobbyPtr = reinterpret_cast<CLobby*>(GetZoneManager().GetZonePtr(_lobbyId));
	_inGameId = GetZoneManager().CreateZone(ZONE_TYPE_INGAME);
	_inGamePtr = reinterpret_cast<CGameZone*>(GetZoneManager().GetZonePtr(_inGameId));
	if (_lobbyId == 0 || _inGameId == 0)
	{
		Log::logging().Log(TAG_LOAD, Log::en_ERROR, L"방 생성 실패 (lobby: %llx, game: %llx)", _lobbyId, _inGameId);
		return false;
	}
	

	return true;
}
bool CGameServer::OnAccept(uint64_t sessionId, in_addr ip, wchar_t* wip)
{
	return GetZoneManager().EnterZone(_lobbyId, sessionId, wip);
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
	Log::logging().Log(TAG_CONTENTS, Log::en_ERROR,
		L"오면 안되는데 ...");
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
	s_toLoginServerClient.Exit();

	if(_lobbyId != 0)
		GetZoneManager().DestroyZone(_lobbyId);
	if (_inGameId != 0)
		GetZoneManager().DestroyZone(_inGameId);
}

// 생성자
CGameServer::CGameServer()
{

}
// 소멸자
CGameServer::~CGameServer()
{

}