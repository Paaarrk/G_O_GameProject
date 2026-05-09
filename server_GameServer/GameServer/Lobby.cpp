#include "Lobby.h"
#include "GameServer.h"
#include "Player.h"
#include "CommonProtocol.h"

#include <chrono>
#include <string>
#include <string_view>
#include <charconv>

#include "RedisConnector.h"
#include "DBRequest.h"


#include "logclassV1.h"
using Log = Core::c_syslog;
using namespace Net;

//---------------------------------------------------------
// Constructor & Destructor
//---------------------------------------------------------

CLobby::CLobby():_useTimeout(true), _signal{ SIGNAL_OFF }, 
_lobbyToRedis(sizeof(stAuthRequest) * LOBBY_RINGBUFFER_SIZE),
_redisToLobby(sizeof(stAuthResponse) * LOBBY_RINGBUFFER_SIZE),
_staticDBPool(CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool())
{
	_lobbyPlayerMap.reserve(GetMaxUsers());
	_loginAccountNoToSessionIdMap.reserve(GetMaxUsers());
	_logoutAccountNoToPlayerMap.reserve(GetMaxUsers());

	_redisThread = std::thread(&CLobby::Redis_ThreadFunc, this);
}
CLobby::~CLobby()
{
	stAuthRequest req;
	req.ExitSignal();
	RequestAuthRedis(&req);
	if (_redisThread.joinable())
		_redisThread.join();
}


//---------------------------------------------------------
// Player Load and Mem
//---------------------------------------------------------

void CLobby::PlayerLoad(CPlayer* player)
{
	player->PlayerWaitLoad(GetTickStartTime());
	CPlayer* befmem = GetLogoutPlayerMemory(player->GetAccountNo());
	if (befmem != nullptr)
	{	// 기존 접속에 관한 메모리가 있음, 바로 이동시키자
		if (player->LoadPlayer(*befmem) == false)
		{
			CPlayer::Free(befmem);
			PlayerDelayLoad(player);
			return;
		}
		CPlayer::Free(befmem);
		GetZoneManager()->MoveZone(GetMyServer()->GetInGameId(), player->GetSessionId());
		CPACKET_CREATE(pkt);
		*pkt << (uint16_t)en_PACKET_CS_GAME_RES_LOGIN
			<< (uint8_t)1
			<< player->GetAccountNo();
		SendPacket(player->GetSessionId(), pkt.GetCPacketPtr());
		return;
	}
	else
	{
		PlayerDelayLoad(player);
	}
}
void CLobby::PlayerDelayLoad(CPlayer* player)
{
	IAsyncRequest* pRequest = new CAsync_GetCharacterInfo(POOL_USE_LOBBY, player->GetSessionId(), player->GetAccountNo(), 
	[this, sessionId = player->GetSessionId()](const GetCharacterInfoResType& res) {
		
		CPlayer* player = FindPlayerInLobby(sessionId);
		if (player == nullptr)
			return;	//이미 나감
		if (player->GetPlayerStatus() != PLAYER_LOGIN_WAIT_LOAD)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] DB Response : Player Load[](){} : Player Status is not 5 (%d)", sessionId, player->GetPlayerStatus());
			Disconnect(sessionId);
			return;
		}
		int64_t accountno = std::get<0>(res);
		if (accountno == 0) // 미발견, 캐릭터 생성 필요
		{
			player->PlayerWaitGameSelect(GetTickStartTime());
			CPACKET_CREATE(pkt);
			*pkt << (uint16_t)en_PACKET_CS_GAME_RES_LOGIN
				 << (uint8_t)2
				 << player->GetAccountNo();
			SendPacket(sessionId, pkt.GetCPacketPtr());
			return;
		}

		// 게임 로드도 성공, 인게임으로 이동 시켜야함
		player->LoadPlayer(res);
		GetZoneManager()->MoveZone(GetMyServer()->GetInGameId(), sessionId);
		CPACKET_CREATE(pkt);
		*pkt << (uint16_t)en_PACKET_CS_GAME_RES_LOGIN
			 << (uint8_t)1
			 << player->GetAccountNo();
		SendPacket(sessionId, pkt.GetCPacketPtr());

	});
	CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(pRequest);
}
CPlayer* CLobby::GetLogoutPlayerMemory(int64_t accountNo)
{
	auto it = _logoutAccountNoToPlayerMap.find(accountNo);
	if (it == _logoutAccountNoToPlayerMap.end())
		return nullptr;
	CPlayer* ret = it->second;
	_logoutAccountNoToPlayerMap.erase(it);
	return ret;
}


//---------------------------------------------------------
// Event Functions
//---------------------------------------------------------

void CLobby::OnUpdate()
{
	unsigned long curTime = GetTickStartTime();
	// 타임아웃 
	if (_useTimeout == true)
	{
		int64_t deltaTime;
		for (std::pair<const uint64_t, CPlayer*>& player : _lobbyPlayerMap)
		{
			deltaTime = GetDeltaTimeMs(curTime, player.second->GetLastRecvedTime());
			if (deltaTime > TIME_OUT_MS_LOBBY)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_SYSTEM, L"[AccountNo: %d] Not Logined, Timeout", player.second->GetAccountNo());
				player.second->PlayerLogout(GetTickStartTime());
				CZone::Disconnect(player.first);
			}
		}
	}

	Check_LoginServerResponse();
	Check_RedisResponse();
	Check_DBResponse();

	// 나간 유저들 메모리 관리 (DB저장 대기 및 일부러 재사용때문에 냅두는 것들)
	for (auto& [accountno, mem] : _logoutAccountNoToPlayerMap)
	{
		int64_t deltaTime = GetDeltaTimeMs(curTime, mem->GetLastRecvedTime());
		if (deltaTime < TIME_OUT_MS_PLAYER_MEMORY) continue;

		if (mem->DBRequestFin())
		{
			_logoutAccountNoToPlayerMap.erase(accountno);
			CPlayer::Free(mem);
		}
		else
		{
			Log::logging().Log(TAG_LOBBY, Log::en_SYSTEM, L"DB 저장이 제대로 안되나 확인 ... 60초지나도 완료가 안됨");
		}
	}
}
void CLobby::OnEnter(uint64_t sessionId, void* playerPtr, std::wstring* ip)
{
	// 신규 로그인인데 포인터가..
	if (playerPtr != nullptr || ip == nullptr)
	{
		Disconnect(sessionId);
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[OnEnter()] Lobby에 포인터가 있거나 IP 를 못받음... (playerptr: %p | ip: %p)", playerPtr, ip);
		return;
	}

	CPlayer* newPlayer = CPlayer::Alloc();
	if (newPlayer == nullptr)
	{
		Disconnect(sessionId);
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[OnEnter()] newPlayer가 nullptr, Alloc()실패");
		return;
	}
	newPlayer->PlayerWaitLogin(GetTickStartTime(), sessionId, *ip);
	auto [it, success] = _lobbyPlayerMap.insert({sessionId, newPlayer});
	if (success == false)
	{
		Disconnect(sessionId);
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[OnEnter()] 세션아이디 관리 문제가 큽니다(맵에 이미 있음)");
		return;
	}

	return;
}
void CLobby::OnLeave(uint64_t sessionId, bool bNeedPlayerDelete)
{
	if (bNeedPlayerDelete)
	{
		CPlayer* player = FindPlayerInLobby(sessionId);
		if (player != nullptr)
		{
			int32_t playerstatus = player->GetPlayerStatus();
			if (EPlayerState::PLAYER_LOGIN <= playerstatus && playerstatus < EPlayerState::PLAYER_IN_GAME)
			{
				// DB Logout찍기
				IAsyncRequest* logoutreq = new CAsync_UpdateLogout(POOL_USE_LOBBY, sessionId, player,
				[](const UpdateLogoutResType& res) {
					// DB반영 실패 -> 나중에 kick요청시 재시도
					// DB반영 성공 -> ok
					// 플레이어 지우기는 나중에 한번에
					// 여기서 할 것은 없음
				}, player->GetAccountNo(), player->GetTileX(), player->GetTileY(), player->GetCristal(), player->GetHp(), GAME_LOBBY_LOGOUT);

				player->IncreaseDBRequest();
				player->PlayerLogout(GetTickStartTime());
				CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(logoutreq);

				_loginAccountNoToSessionIdMap.erase(sessionId);
				auto [it, success] = _logoutAccountNoToPlayerMap.insert({ player->GetAccountNo(), player });
				if (!success)
				{
					Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"Lobby::Onleave() 로그인 두개 받은건지 체크... 재사용 코드에서 안뺏는지 체크...");
				}
				
			}
			else // 로그인이 아닌 연결
			{
				CPlayer::Free(player);
			}
		}
	}

	_lobbyPlayerMap.erase(sessionId);
}
void CLobby::OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen)
{
	uint16_t type = RequestCheckType(readPtr, payloadlen);
	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		if (RequestLogin(sessionId, readPtr, payloadlen) == false)
		{
			Disconnect(sessionId);	// 사유는 함수 내부에서
		}
		break;
	
	case en_PACKET_CS_GAME_REQ_CHARACTER_SELECT:
		if (RequestCharacterSelect(sessionId, readPtr, payloadlen) == false)
		{
			Disconnect(sessionId);	// 사유는 함수 내부에서
		}
		break;

	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		if (RequestHeartbeat(sessionId, readPtr, payloadlen) == false)
		{
			Disconnect(sessionId);
		}
		break;

	default:
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Strange Packet Type: %d", type);
		Disconnect(sessionId);
		break;
	}
}

//---------------------------------------------------------
// Packets
//---------------------------------------------------------

bool CLobby::RequestLogin(uint64_t sessionId, const char* readptr, int32_t payloadlen)
{
	int64_t accountNo;
	char sessionKey64[SESSION_KEY_LEN];
	int32_t version;

	if (payloadlen != SizeOf(accountNo, sessionKey64, version))
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestLogin(), Packet Payloadlen Error(%d is short)", sessionId, payloadlen);
		return false;
	}
	CPlayer* player = FindPlayerInLobby(sessionId);
	if (player == nullptr)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestLogin(), is logined or not connected", sessionId);
		return false;
	}
	if (player->GetPlayerStatus() != EPlayerState::PLAYER_WAIT_LOGIN_PACKET)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestLogin(), Duplicate Login Packet ", sessionId);
		return false;
	}

	memcpy(&accountNo, readptr, sizeof(accountNo)); 
	readptr += sizeof(accountNo);
	memcpy(&sessionKey64, readptr, sizeof(sessionKey64));
	readptr += sizeof(sessionKey64);
	memcpy(&version, readptr, sizeof(version));
	readptr += sizeof(version);

	player->PlayerWaitRedisCheck(GetTickStartTime(), accountNo, sessionKey64, version);
	
	// Redis 스레드에 요청 보냄
	char ip[IPV4_LEN];
	WideCharToMultiByte(CP_UTF8, 0, player->GetPlayerIp(), -1, ip, IPV4_LEN, NULL, NULL);
	stAuthRequest redisreq(sessionId, accountNo, sessionKey64);
	if (RequestAuthRedis(&redisreq) == false)
	{
		// 큐가 꽉참, 처리가 늦어지는중
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestLogin(), Cannot Login because of RingBuffer full");
		return false;
	}

	return true;
}
bool CLobby::RequestCharacterSelect(uint64_t sessionId, const char* readptr, int32_t payloadlen)
{
	uint8_t charType;
	if (payloadlen != SizeOf(charType))
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestCharacterSelect(), Packet Payloadlen Error(%d is strange(%d))", sessionId, payloadlen, SizeOf(charType));
		return false;
	}
	CPlayer* player = FindPlayerInLobby(sessionId);
	if (player == nullptr)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestCharacterSelect(), is logined or not connected", sessionId);
		return false;
	}
	if (player->GetPlayerStatus() != EPlayerState::PLAYER_LOGIN_GAME_SELECT)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestCharacterSelect(), Duplicate Login Packet ", sessionId);
		return false;
	}

	memcpy(&charType, readptr, sizeof(charType));
	readptr += sizeof(charType);
	
	if ('1' < charType || charType > '5')
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestCharacterSelect(), charType is not ranged [1, 5](%d)", sessionId, (int)(charType - '0'));
		return false;
	}


	player->SetPlayerFirstCreate(static_cast<int32_t>(charType - '0'), CStaticDatas::Player_Hp());
	player->PlayerWaitSelectDBSave(GetTickStartTime());
	IAsyncRequest* req = new CAsync_InsertNewCharacter(EDBPoolUse::POOL_USE_LOBBY, sessionId, player, [this, sessionId](const InsertNewCharacterResType& res){
		const auto& [accountno, success] = res;

		if (!success)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx | accountno: %lld] ResponseCharacterSelect() 플레이어 새 캐릭터 생성 DB 실패", sessionId, accountno);
			Disconnect(sessionId);
			return;
		}

		CPlayer* player = this->FindPlayerInLobby(sessionId);
		if (player == nullptr)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] ResponseCharacterSelect(), 이미 나간 플레이어", sessionId);
			Disconnect(sessionId);
			return;
		}
		if (player->GetPlayerStatus() != PLAYER_LOGIN_WAIT_SELECT_DB_SAVE)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] ResponseCharacterSelect() 플레이어 상태 %d로 이상함(%d)", player->GetPlayerStatus(), PLAYER_LOGIN_WAIT_SELECT_DB_SAVE);
			Disconnect(sessionId);
			return;
		}

		// 바로 존 이동
		GetZoneManager()->MoveZone(GetMyServer()->GetInGameId(), sessionId);

	}, player->GetAccountNo(), charType, SERVER_GAME);
	player->IncreaseDBRequest(); 
	CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(req);

	return true;
}
bool CLobby::RequestHeartbeat(uint64_t sessionId, const char* readptr, int32_t payloadlen)
{
	if (payloadlen != SizeOf())
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestHeartbeat(), Packet Payloadlen Error(%d is strange(%d))", sessionId, payloadlen, SizeOf());
		return false;
	}


	return true;
}


//------------------------------------------
// Redis
//------------------------------------------

bool CLobby::RequestAuthRedis(const stAuthRequest* req)
{
	int size = _lobbyToRedis.Enqueue(reinterpret_cast<const char*>(req), sizeof(*req));
	if (size != sizeof(*req))
		return false;
	_signal.exchange(SIGNAL_ON, std::memory_order_seq_cst);
	_signal.notify_one();
	return true;
}
CPlayer* CLobby::ResponseAuthRedis(const stAuthResponse* res) const
{
	CPlayer* player = FindPlayerInLobby(res->sessionId);
	if (player == nullptr)
	{
		return nullptr;	// 이미 나간 플레이어
	}
	if (player->GetPlayerStatus() != EPlayerState::PLAYER_WAIT_REDIS_CHECKING)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx]ResponseAuthRedis(), Redis 대기 상태가 아님 [status %d != 1]",
			res->sessionId, player->GetPlayerStatus());
		return nullptr;
	}
	return player;
}
void CLobby::Check_RedisResponse()
{
	stAuthResponse res;
	while (_redisToLobby.Dequeue(reinterpret_cast<char*>(&res), sizeof(res)) == sizeof(res))
	{
		CPlayer* player = FindPlayerInLobby(res.sessionId);
		if (player == nullptr)
			continue;	//이미 나감
		if (player->GetPlayerStatus() != PLAYER_WAIT_REDIS_CHECKING)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] OnUpdate()-RedisCheck : Player Status is not 1 (%d)", res.sessionId,  player->GetPlayerStatus());
			Disconnect(res.sessionId);
			continue;
		}
		
		// 상태 변경
		IAsyncRequest* req = new CAsync_GetAccountInfo(POOL_USE_LOBBY, res.sessionId, player->GetAccountNo(), 
		[this, sessionId = res.sessionId](const GetAccountInfoResType& res) {
			
			int64_t accountno = std::get<0>(res);
			const stName& nick_name = std::get<2>(res);
			int32_t status = std::get<3>(res);

			// DB 결과물 확인
			if (status == 0)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] CAsync_GetAccountInfo::ResponseProcess() : Player enter here without enter Login Server (account: status Logout)", sessionId);
				Disconnect(sessionId);
				return;
			}
			if (this->FindLoginSession(accountno) != 0)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] CAsync_GetAccountInfo::ResponseProcess() : Player enter here without enter Login Server (account: status Logout but Gameserver exist)", sessionId);
				Disconnect(sessionId);
				return;
			}
			// Player 확인
			CPlayer* player = this->FindPlayerInLobby(sessionId);
			if (player == nullptr)
				return;	//이미 나감
			if (player->GetPlayerStatus() != PLAYER_WAIT_DB_CHECKING)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] CAsync_GetAccountInfo::ResponseProcess() : Player Status is not 3 (%d)", sessionId, player->GetPlayerStatus());
				Disconnect(sessionId);
				return;
			}
			
			// 확인이 끝났으면 로그인 처리
			player->SetPlayerName(nick_name);
			if (this->InsertPlayerSession(accountno, sessionId) == false)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] CAsync_GetAccountInfo::ResponseProcess() : 맵 넣기 실패... 위에서 확인 했는데 나오면 안됨", sessionId);
				Disconnect(sessionId);
				return;
			}

			// 다음으로 캐릭터 정보 읽어오기
			this->PlayerLoad(player);
		});

		player->PlayerWaitDbCheck(GetTickStartTime(), res.sessionId);
		CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(req);
	}
}


//----------------------------------
// DB
//----------------------------------

void CLobby::Check_DBResponse()
{
	IAsyncRequest* request;
	while ((request = _staticDBPool.GetResponse<POOL_USE_LOBBY>()) != nullptr)
	{
		request->ResponseProcess();
		delete request;
	}
}

//--------------------------------------
// From LoginServer
//--------------------------------------

void CLobby::Check_LoginServerResponse()
{
	Core::CLockFreeQueue<Net::CPacket*>& q = _fromLoginQ;
	Net::CPacket* pPacket;
	while (q.Dequeue_Single(pPacket) == true)
	{
		uint16_t type;
		*pPacket >> type;
		switch (type)
		{
		case en_PACKET_SS_REQ_NEW_CLIENT_LOGIN:
			Login_RequestNewClientLogin(pPacket);
			break;
		default:
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"Loginserver response 에 이상이 있음");
			break;
		}

		Net::CPacket::Free(pPacket);
	}
}
void CLobby::Login_RequestNewClientLogin(Net::CPacket* packet)
{
	int16_t type;
	int64_t accountno;
	uint64_t sequence;
	if (packet->GetDataSize() != SizeOf(accountno, sequence))
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"로그인 서버로 부터 온 패킷 이상함. 디버깅 바람 (길이: %d != %d)",
			packet->GetDataSize(), SizeOf(accountno, sequence));
		return;
	}

	*packet >> accountno;
	*packet >> sequence;
	uint64_t sessionId;
	if ((sessionId = FindLoginSession(accountno)) != 0)
	{	// 누가 게임중, 킥 준비, Logout이 안찍혀서 킥 해달라고 패킷이 온 상황
		Log::logging().Log(TAG_LOBBY, Log::en_SYSTEM, L"[sessionId: %016llx] 새로운 세션 로그인으로 받을 준비하기 위한 킥", sessionId);
		Disconnect(sessionId);
	}

	// 패킷 재활용 준비
	uint16_t* typeptr = (uint16_t*)packet->ReuseSSProtocol(SizeOf(type, accountno, sequence));
	*typeptr = en_PACKET_SS_RES_NEW_CLIENT_LOGIN;
	// 내부에서 참조 올림, 밖에서 똑같이 참조 해제
	CGameServer::GetLoginServerConnection().SendPacket(packet);
	
}


//--------------------------------------
// Redis Thread
//--------------------------------------

void CLobby::Redis_ThreadFunc()
{
	bool fin = false;
	stAuthRequest req;
	while (fin == false)
	{
		_signal.wait(SIGNAL_OFF, std::memory_order_seq_cst);
		// 링버퍼 로직
		while (_lobbyToRedis.Dequeue(reinterpret_cast<char*>(&req), sizeof(req)) == sizeof(req))
		{
			if (req.sessionId == 0)
			{
				fin = true;
				break;
			}
			Redis_Process(req);
		}

		_signal.exchange(0, std::memory_order_seq_cst);
		// (넣고 1로 바꿈) -> 내가 0으로 바꿈 (큐에는 1개 남고) 이런 상황 방지
		while (_lobbyToRedis.Dequeue(reinterpret_cast<char*>(&req), sizeof(req)) == sizeof(req))
		{
			if (req.sessionId == 0)
			{
				fin = true;
				break;
			}
			Redis_Process(req);
		}
	}

	return;
}
void CLobby::Redis_Process(const stAuthRequest& req)
{
	CRedisConnector& conn = CGameServer::GetRedisConnector();
	std::string key(std::to_string(req.accountNo));
	//key += ':';
	//key += req.ip;
	std::string value(conn.GetValue(key));
	Redis_MakeResponse(req, value);
}
void CLobby::Redis_MakeResponse(const stAuthRequest& req, const std::string& value)
{
	// 못찾음
	if (value.length() == 0)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Redis-Redis_MakeResponse(), accountno not found", req.sessionId);
		Disconnect(req.sessionId);
		return;
	}
	// 세션키 다름
	if (memcmp(req.sessionKey64, value.c_str(), SESSION_KEY_LEN) != 0)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Redis-Redis_MakeResponse(), sessionKey diff", req.sessionId);
		Disconnect(req.sessionId);
		return;
	}

	stAuthResponse res(req.sessionId);
	while (_redisToLobby.Enqueue(reinterpret_cast<const char*>(&res), sizeof(res)) != sizeof(res))
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"Redis-MakeReponse(), Lobby cannot process redis response normally");

	return;
}

