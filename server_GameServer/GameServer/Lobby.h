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


	//---------------------------------------------------------
	// Event Functions
	//---------------------------------------------------------

	virtual void OnUpdate();
	virtual void OnEnter(uint64 sessionId, void* playerPtr, std::wstring* ip);
	virtual void OnLeave(uint64 sessionId, bool bNeedPlayerDelete);
	virtual void OnMessage(uint64 sessionId, const char* readPtr, int payloadlen);

private:
	std::unordered_map<uint64_t, CPlayer*> _players;
	
};

#endif