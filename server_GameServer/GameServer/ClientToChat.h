#ifndef __CLIENT_TO_CHAT
#define __CLIENT_TO_CHAT
#include "NetClient.h"

enum ECLlientToChat
{
	RECONNECT_TRY_CNT = 3,
};

class CClientToChat : public Net::CClient
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
