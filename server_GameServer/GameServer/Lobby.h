#ifndef __LOBBY_H__
#define __LOBBY_H__
#include "Zone.h"
#include <unordered_map>
#include <thread>
#include <atomic>
#include "RingBufferV4.h"
#include "ClientToLogin.h"
#include "LockFreeQueue.hpp"
#include "Contents.h"

#include "DBThreadPool.h"

class CPlayer;
class CGameServer;

struct stAuthRequest
{
	uint64_t	sessionId;
	int64_t		accountNo;
	char		sessionKey64[SESSION_KEY_LEN];
	char		ip[IPV4_LEN];

	void ExitSignal() noexcept { sessionId = 0; }
#pragma warning(push)
#pragma warning(disable: 26495)	// 초기화 필요성 없음
	stAuthRequest() noexcept {}
#pragma warning(pop)
	stAuthRequest(uint64_t sessionid, int64_t accno, char(&skey)[SESSION_KEY_LEN], char(&ip)[IPV4_LEN]) noexcept
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

	// 0 리턴시 게임중X, 세션아이디 리턴시 게임중
	uint64_t FindLoginSession(int64_t accountNo) const
	{
		auto it = _loginAccountNoToSessionIdMap.find(accountNo);
		if (it == _loginAccountNoToSessionIdMap.end())
			return 0;
		return it->second;
	}

	bool InsertPlayerSession(int64_t accountNo, uint64_t sessionId)
	{
		auto [it, success] = _loginAccountNoToSessionIdMap.insert({ accountNo, sessionId });
		return success;
	}
	
	//---------------------------------------------------------
	// Player Load and Mem
	//---------------------------------------------------------

	void PlayerLoad(CPlayer* player);
	void PlayerDelayLoad(CPlayer* player);
	// 내부에서 지우고 그 포인터를 돌려줌 (사용 후 풀에 반환 필요)
	CPlayer* GetLogoutPlayerMemory(int64_t accountNo);

	//---------------------------------------------------------
	// Event Functions
	//---------------------------------------------------------

	virtual void OnUpdate();
	virtual void OnEnter(uint64_t sessionId, void* playerPtr, std::wstring* ip);
	virtual void OnLeave(uint64_t sessionId, bool bNeedPlayerDelete);
	virtual void OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen);

	//---------------------------------------------------------
	// Get
	//---------------------------------------------------------

	CGameServer* GetMyServer() const;
private:
	uint16_t CheckType(const char*& readPtr, int32_t& len) noexcept
	{
		if (len < MESSAGE_HEADER_LEN)
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


	//----------------------------------
	// From Login Server
	//----------------------------------
	void CheckLoginServerResponses();
	void RequestNewClientLogin(Net::CPacket* packet);
	

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
	// DB
	//----------------------------------
	
	void CheckDBResponses();


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

	// 계정이랑 엮이지도 않고 정말 메모리만 덩그러니 있는 상태,
	// 단, DB에 저장중 일 수는 있음
	std::unordered_map<int64_t, CPlayer*> _logoutAccountNoToPlayerMap;
	
	bool _useTimeout;

	std::thread _redisThread;
	Core::RingBuffer _lobbyToRedis;
	Core::RingBuffer _redisToLobby;
	std::atomic<int32_t> _signal;

	Core::CLockFreeQueue<Net::CPacket*> _fromLoginQ;
	
	CDBThreadPool<POOL_USE_COUNT>& _staticDBReadPool;
};


#endif