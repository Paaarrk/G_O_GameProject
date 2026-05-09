#include "GameZone.h"


#include "logclassV1.h"
using Log = Core::c_syslog;

CGameZone::CGameZone()
{
	_gamePlayerMap.reserve(GetMaxUsers());
}

CGameZone::~CGameZone()
{
	
}


//---------------------------------------------------------
// Event Functions
//---------------------------------------------------------


void CGameZone::OnUpdate()
{
	
}
void CGameZone::OnEnter(uint64_t sessionId, void* playerPtr, std::wstring* ip)
{
	CPlayer* player = reinterpret_cast<CPlayer*>(playerPtr);
	if (player == nullptr)
	{
		Log::logging().Log(TAG_GAME, Log::en_ERROR, L"[sessionId: %016llx] GameZone::OnEnter(), Playerptr이 null입니다");
		Disconnect(sessionId);
		return;
	}

	
}
void CGameZone::OnLeave(uint64_t sessionId, bool bNeedPlayerDelete)
{

}
void CGameZone::OnMessage(uint64_t sessionId, const char* readPtr, int payloadlen)
{

}
