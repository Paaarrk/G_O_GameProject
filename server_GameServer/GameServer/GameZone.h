#ifndef __GAME_ZONE_H__
#define __GAME_ZONE_H__

#include "Zone.h"
#include <unordered_map>
#include <atomic>
#include "DBWriter.h"
#include "LockFreeQueue.hpp"
#include "Contents.h"

class CPlayer;
class CGameZone : public Net::CZone
{
public:
	CGameZone();
	~CGameZone();

	//---------------------------------------------------------
	// Event Functions
	//---------------------------------------------------------

	void OnUpdate() override;
	void OnEnter(uint64_t sessionId, void* playerPtr, std::wstring* ip) override;
	void OnLeave(uint64_t sessionId, bool bNeedPlayerDelete) override;
	void OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen) override;
	
private:
	std::unordered_map<uint64_t, CPlayer*> _gamePlayerMap;

	CDBWriterThread _mapwriter;
};

#endif