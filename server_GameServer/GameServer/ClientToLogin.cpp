#include "ClientToLogin.h"
#include "Contents.h"

#include "logclassV1.h"
using Log = Core::c_syslog;

bool CClientToLoginServer::OnInit()
{
	// 클라 연결 시도
	if (Connect(RECONNECT_TRY_CNT) == false)
		Log::logging().Log(TAG_TO_LOGIN, Log::en_ERROR, L"CClientToLoginServer::OnInit(), Cannot Connect LoginServer");
	
	
	return true;
}

void CClientToLoginServer::OnExit()
{

}

void CClientToLoginServer::OnEnterJoinServer()
{

}

void CClientToLoginServer::OnLeaveServer()
{
	while (Connect(RECONNECT_TRY_CNT) == false)
	{
		Log::logging().Log(TAG_TO_LOGIN, Log::en_ERROR, L"OnLeaveServer(): Cannot Connect Chat Server");
	}
}

void CClientToLoginServer::OnMessage(Net::CPacket* pPacket, int len)
{

}