#ifndef __LOBBY_H__
#define __LOBBY_H__
#include "Zone.h"
#include <unordered_map>
#include <thread>
#include <atomic>
#include "RingBufferV4.h"
#include "ClientToChat.h"
#include "LockFreeQueue.hpp"
#include "Contents.h"

class CPlayer;

struct stAuthRequest
{
	uint64_t	sessionId;
	int64_t		accountNo;
	char		sessionKey64[SESSION_KEY_LEN];
	char		ip[IPV4_LEN];
#pragma warning(push)
#pragma warning(disable: 26495)	// 초기화 필요성 없음
	stAuthRequest() {}
#pragma warning(pop)
	stAuthRequest(uint64_t sessionid, int64_t accno, char(&skey)[SESSION_KEY_LEN], char(&ip)[IPV4_LEN])
		:sessionId(sessionid), accountNo(accno)
	{
		memcpy(sessionKey64, skey, SESSION_KEY_LEN);
		memcpy(this->ip, ip, IPV4_LEN);
	}
};

struct stAuthResponse
{
	uint64_t sessionId;
	int32_t gameserverId;
	int32_t chatserverId;
	uint64_t sequence;
};

class CLobby : public Net::CZone
{
public:
	CLobby();
	~CLobby();
	
	CPlayer* FindPlayerInLobby(uint64_t sessionId) const
	{
		auto it = _lobbyPlayerMap.find(sessionId);
		if (it == _lobbyPlayerMap.end())
			return nullptr;
		return it->second;
	}
	uint64_t FindLoginSession(int64_t accountNo) const
	{
		auto it = _loginAccountNoToSessionIdMap.find(accountNo);
		if (it == _loginAccountNoToSessionIdMap.end())
			return 0;
		return it->second;
	}

	// 교환 했으면 기존 세션id, 새로 붙였으면 0
	uint64_t ExchangeLoginSession(int64_t accountNo, uint64_t sessionId)
	{
		auto it = _loginAccountNoToSessionIdMap.find(accountNo);
		if (it == _loginAccountNoToSessionIdMap.end())
		{
			_loginAccountNoToSessionIdMap.insert({ accountNo, sessionId });
			return false;
		}
		// 이미 로그인 된 유저가 있음
		uint64_t befId = it->second;
		it->second = sessionId;
		return befId;
	}

	CPlayer* FindLogoutPlayerObject(int64_t accountNo) const
	{
		auto it = _logoutAccountNoToPlayerMap.find(accountNo);
		if (it == _logoutAccountNoToPlayerMap.end())
			return nullptr;
		return it->second;
	}

	//---------------------------------------------------------
	// Event Functions
	//---------------------------------------------------------

	virtual void OnUpdate();
	virtual void OnEnter(uint64_t sessionId, void* playerPtr, std::wstring* ip);
	virtual void OnLeave(uint64_t sessionId, bool bNeedPlayerDelete);
	virtual void OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen);


private:
	uint16_t CheckType(const char*& readPtr, int32_t& len) noexcept
	{
		if (len < sizeof(uint16_t))
			return 0;
		uint16_t type;
		memcpy(&type, readPtr, sizeof(uint16_t));
		readPtr += sizeof(uint16_t);
		len -= sizeof(uint16_t);
		return type;
	}

	//----------------------------------
	// Packets
	//----------------------------------

	bool RequestLogin(uint64_t sessionId, const char* readptr, int32_t payloadlen);
	void RequestDefault(uint16_t type, const char* readptr, int32_t payloadlen);

	//----------------------------------
	// Redis
	//----------------------------------

	bool RequestAuthRedis(const stAuthRequest* req);
	// 진행 불가능 nullptr, 진행가능 CPlayer*
	// 레디스에서 성공했을 때만 답이 옴
	CPlayer* ResponseAuthRedis(const stAuthResponse* res) const;
	// 레디스 큐 비우기
	void CheckRedisResponses();

	//----------------------------------
	// Master (Chat Server)
	//----------------------------------
	// 마스터 큐 비우기
	void CheckMasterResponses();
	void ResponseMasterAccept(Net::CPacket* packet);


	//----------------------------------
	// Redis Thread Func
	//----------------------------------
	void RedisThreadFunc();
	void ProcessRedis(const stAuthRequest& req);
	void MakeResponse(const stAuthRequest& req, const std::string& value);
	int32_t GetLtoR_QSize() const { return _lobbyToRedis.GetUseSize() / sizeof(stAuthRequest); }
	int32_t GetRtoL_QSize() const { return _redisToLobby.GetUseSize() / sizeof(stAuthResponse); }

	std::unordered_map<uint64_t, CPlayer*> _lobbyPlayerMap;
	std::unordered_map<int64_t, uint64_t> _loginAccountNoToSessionIdMap;
	std::unordered_map<int64_t, CPlayer*> _logoutAccountNoToPlayerMap;
	bool _useTimeout;

	std::thread _redisThread;
	Core::RingBuffer _lobbyToRedis;
	Core::RingBuffer _redisToLobby;
	std::atomic<int32_t> _signal;

	CClientToChat _toMasterClient;
	Core::CLockFreeQueue<Net::CPacket*> _fromMasterQ;
};


#endif