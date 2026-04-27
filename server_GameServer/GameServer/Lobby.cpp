#include "Lobby.h"
#include "GameServer.h"
#include "Player.h"
#include "CommonProtocol.h"

#include <chrono>
#include <string>
#include <string_view>
#include <charconv>

#include "RedisConnector.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

CLobby::CLobby():_useTimeout(true), _signal{ SIGNAL_OFF }, 
_lobbyToRedis(sizeof(stAuthRequest) * LOBBY_RINGBUFFER_SIZE),
_redisToLobby(sizeof(stAuthResponse) * LOBBY_RINGBUFFER_SIZE)
{
	_lobbyPlayerMap.reserve(GetMaxUsers());

	_loginAccountNoToSessionIdMap.reserve(GetMaxUsers());
	_logoutAccountNoToPlayerMap.reserve(GetMaxUsers());

	_redisThread = std::thread(&CLobby::RedisThreadFunc, this);
}
CLobby::~CLobby()
{
	
}

void CLobby::OnUpdate()
{
	// 타임아웃 
	if (_useTimeout == true)
	{
		TimePoint curTime= SteadyClock::now();
		int64_t deltaTime;
		for (std::pair<const uint64_t, CPlayer*>& player : _lobbyPlayerMap)
		{
			deltaTime = GetDeltaTimeMs(curTime, player.second->GetLastRecvedTime());
			if (deltaTime > TIME_OUT_MS_LOBBY)
			{
				Log::logging().Log(TAG_LOBBY, Log::en_SYSTEM, L"[AccountNo: %d] Not Logined, Timeout", player.second->GetAccountNo());
				player.second->PlayerLogout();
				CZone::Disconnect(player.first);
			}
		}
	}

	CheckRedisResponses();

	CheckMasterResponses();
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
	newPlayer->PlayerWaitLogin(sessionId, *ip);
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
	
}
void CLobby::OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen)
{
	uint16_t type = CheckType(readPtr, payloadlen);
	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		if (RequestLogin(sessionId, readPtr, payloadlen) == false)
		{
			Disconnect(sessionId);	// 사유는 함수 내부에서
		}
		break;
	default:
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Strange Packet Type: %d", type);
		Disconnect(sessionId);
		break;
	}
}

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

	memcpy(&accountNo, &readptr, sizeof(accountNo)); 
	readptr += sizeof(accountNo);
	memcpy(&sessionKey64, &readptr, sizeof(sessionKey64));
	readptr += sizeof(sessionKey64);
	memcpy(&version, &readptr, sizeof(version));
	readptr += sizeof(version);

	player->PlayerWaitRedisCheck(accountNo, sessionKey64, version);
	
	// Redis 스레드에 요청 보냄
	char ip[IPV4_LEN];
	WideCharToMultiByte(CP_UTF8, 0, player->GetPlayerIp(), -1, ip, IPV4_LEN, NULL, NULL);
	stAuthRequest redisreq(sessionId, accountNo, sessionKey64, ip);
	if (RequestAuthRedis(&redisreq) == false)
	{
		// 큐가 꽉참, 처리가 늦어지는중
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] RequestLogin(), Cannot Login because of RingBuffer full");
		return false;
	}

	return true;
}
void CLobby::RequestDefault(uint16_t type, const char* readptr, int32_t payloadlen)
{
	
}


//------------------------------------------
// Redis
//------------------------------------------

bool CLobby::RequestAuthRedis(const stAuthRequest* req)
{
	int size = _lobbyToRedis.Enqueue(reinterpret_cast<const char*>(req), sizeof(*req));
	if (size != sizeof(*req))
		return false;
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

void CLobby::CheckRedisResponses()
{
	stAuthResponse res;
	while (_redisToLobby.Dequeue(reinterpret_cast<char*>(&res), sizeof(res)) == sizeof(res))
	{
		CPlayer* player = FindPlayerInLobby(res.sessionId);
		if (player == nullptr)
			continue;	//이미 나감
		if (player->GetPlayerStatus() != PLAYER_WAIT_REDIS_CHECKING)
		{
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"OnUpdate()-RedisCheck : Player Status is not 1 (%d)", player->GetPlayerStatus());
			Disconnect(res.sessionId);
			continue;
		}
		
		// 상태 변경
		player->PlayerWaitMasterAccept(res.sequence, res.gameserverId, res.chatserverId);
		CPACKET_CREATE(requestAcceptPacket);
		*requestAcceptPacket << static_cast<uint16_t>(en_PACKET_SS_NEW_USER_REQUEST);
		*requestAcceptPacket << player->GetSessionId();
		*requestAcceptPacket << player->GetAccountNo();
		*requestAcceptPacket << res.sequence;
		*requestAcceptPacket << res.chatserverId;
		*requestAcceptPacket << res.gameserverId;

		// 채팅서버(마스터) 에 요청
		_toMasterClient.SendPacket(requestAcceptPacket.GetCPacketPtr());
	}
}


//--------------------------------------
// Master (Chat Server)
//--------------------------------------

void CLobby::CheckMasterResponses()
{
	Core::CLockFreeQueue<Net::CPacket*>& q = _fromMasterQ;
	Net::CPacket* pPacket;
	while (q.Dequeue_Single(pPacket) == true)
	{
		uint16_t type;
		*pPacket >> type;
		switch (type)
		{
		case en_PACKET_SS_NEW_USER_RESPONSE:
			ResponseMasterAccept(pPacket);
			break;
		default:
			Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"Master Response에 이상이 있음");
			break;
		}

		Net::CPacket::Free(pPacket);
	}
}

void CLobby::ResponseMasterAccept(Net::CPacket* packet)
{
	uint64_t sessionId;
	int64_t accountNo;
	uint64_t sequence;
	int8_t success;
	if (packet->GetDataSize() != SizeOf(sessionId, accountNo, sequence, success))
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"ResponseMasterAccept(), 패킷 길이 이상 (me: %d != %d)",
			packet->GetDataSize(), SizeOf(sessionId, accountNo, sequence, success));
		// 패킷도 안꺼내서 끊을 수도 없음.
		return;
	}
	*packet >> sessionId;
	*packet >> accountNo;
	*packet >> sequence;
	*packet >> success;
	
	if (success == false)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] ResponseMasterAccept(), 로그인 실패함", sessionId);
		Disconnect(sessionId);
		return;
	}

	CPlayer* pPlayer = FindPlayerInLobby(sessionId);
	if (pPlayer == nullptr)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] ResponseMasterAccept(), 이미 없는 유저", sessionId);
		return;
	}

	if (pPlayer->GetPlayerStatus() != EPlayerState::PLAYER_WAIT_MASTER_ACCEPT)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] ResponseMasterAccept(), 상태 이상함 (%d != %d (정상상태))",
			pPlayer->GetPlayerStatus(), EPlayerState::PLAYER_WAIT_MASTER_ACCEPT);
		Disconnect(sessionId);
		return;
	}

	uint64_t retId = ExchangeLoginSession(accountNo, sessionId);
	if (retId == 0)
	{	
		// 한번 잔여 메모리 확인 후 없으면 db읽음
		// 있으면 그냥 사용
	}
	else
	{
		// 곧 반환 될 메모리 기다림
	}
}	

//--------------------------------------
// Redis Thread
//--------------------------------------

void CLobby::RedisThreadFunc()
{
	stAuthRequest req;
	while (1)
	{
		_signal.wait(SIGNAL_OFF, std::memory_order_seq_cst);
		// 링버퍼 로직
		while (_lobbyToRedis.Dequeue(reinterpret_cast<char*>(&req), sizeof(req)) == sizeof(req))
		{
			ProcessRedis(req);
		}

		_signal.exchange(0, std::memory_order_seq_cst);
		// (넣고 1로 바꿈) -> 내가 0으로 바꿈 (큐에는 1개 남고) 이런 상황 방지
		while (_lobbyToRedis.Dequeue(reinterpret_cast<char*>(&req), sizeof(req)) == sizeof(req))
		{
			ProcessRedis(req);
		}
	}

	return;
}

void CLobby::ProcessRedis(const stAuthRequest& req)
{
	CRedisConnector& conn = CGameServer::GetRedisConnector();
	std::string key(std::to_string(req.accountNo));
	key += ':';
	key += req.ip;
	std::string value(conn.GetValue(key));
	MakeResponse(req, value);
}

void CLobby::MakeResponse(const stAuthRequest& req, const std::string& value)
{
	// 못찾음
	if (value.length() == 0)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Redis-MakeResponse(), accountno not found", req.sessionId);
		Disconnect(req.sessionId);
		return;
	}
	// 세션키 다름
	if (memcmp(req.sessionKey64, value.c_str(), SESSION_KEY_LEN) != 0)
	{
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"[sessionId: %016llx] Redis-MakeResponse(), sessionKey diff", req.sessionId);
		Disconnect(req.sessionId);
		return;
	}
	const char* start = value.c_str() + SESSION_KEY_LEN;
	const char* end = start + value.length() - SESSION_KEY_LEN;
	const char* curend = ++start;	//처음 세션 키 이후 : 건너뛰기

	// 세션키:채팅서버번호:게임서버번호:시퀀스:
	std::string_view view[3];
	int i = 0;
	while (curend != end || i < 3)
	{
		if (*curend == ':')
		{
			view[i++] = {start, static_cast<size_t>(curend - start)};
			start = curend + 1;
		}
		++curend;
	}

	stAuthResponse res;
	res.sessionId = req.sessionId;
	std::from_chars(view[0].data(), view[0].data() + view[0].size(), res.chatserverId);
	std::from_chars(view[1].data(), view[1].data() + view[1].size(), res.gameserverId);
	std::from_chars(view[2].data(), view[2].data() + view[2].size(), res.sequence);
	
	while (_redisToLobby.Enqueue(reinterpret_cast<const char*>(&res), sizeof(res)) != sizeof(res))
		Log::logging().Log(TAG_LOBBY, Log::en_ERROR, L"Redis-MakeReponse(), Lobby cannot process redis response normally");

	return;
}

