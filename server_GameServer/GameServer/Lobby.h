#ifndef __LOBBY_H__
#define __LOBBY_H__
#include "Zone.h"
#include <unordered_map>

class CPlayer;
class CLobby : public Net::CZone
{
public:
	CLobby();
	~CLobby();
	
	CPlayer* FindPlayer(uint64_t sessionId)
	{
		auto it = _playerMap.find(sessionId);
		if (it == _playerMap.end())
			return nullptr;
		return it->second;
	}

	//---------------------------------------------------------
	// Event Functions
	//---------------------------------------------------------

	virtual void OnUpdate();
	virtual void OnEnter(uint64 sessionId, void* playerPtr, std::wstring* ip);
	virtual void OnLeave(uint64 sessionId, bool bNeedPlayerDelete);
	virtual void OnMessage(uint64 sessionId, const char* readPtr, int payloadlen);


private:

	std::unordered_map<uint64_t, CPlayer*> _playerMap;
	std::unordered_map<int64_t, uint64_t> _accountIdToSessionIdMap;
	std::unordered_map<int64_t, CPlayer*> _accountIdToPlayerMap;
	bool useTimeout;
};

#endif