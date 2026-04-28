#ifndef __CLIENT_TO_LOGIN
#define __CLIENT_TO_LOGIN
#include "NetClient.h"

enum EClientToLoginServer
{
	RECONNECT_TRY_CNT = 3,
};

class CClientToLoginServer : public Net::CClient
{
public:
	virtual bool OnInit();
	virtual void OnExit();

	virtual void OnEnterJoinServer();
	virtual void OnLeaveServer();
	virtual void OnMessage(Net::CPacket* pPacket, int len);
private:
	
};

#endif
