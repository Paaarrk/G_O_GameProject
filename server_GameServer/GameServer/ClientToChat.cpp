#include "ClientToChat.h"
#include "Contents.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

bool CClientToChat::OnInit()
{
	// 클라 연결 시도
	while (Connect(RECONNECT_TRY_CNT) == false)
		Log::logging().Log(TAG_TO_CHAT, Log::en_ERROR, L"CClientToChat::OnInit(), Cannot connect chatserver");
	
	
	return true;
}

void CClientToChat::OnExit()
{

}

void CClientToChat::OnEnterJoinServer()
{

}

void CClientToChat::OnLeaveServer()
{
	while (Connect(RECONNECT_TRY_CNT) == false)
	{
		Log::logging().Log(TAG_TO_CHAT, Log::en_ERROR, L"OnLeaveServer(): Cannot Connect Chat Server");
	}
}

void CClientToChat::OnMessage(Net::CPacket* pPacket, int len)
{

}