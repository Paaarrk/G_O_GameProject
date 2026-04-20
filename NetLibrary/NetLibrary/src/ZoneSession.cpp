#include "ZoneSession.h"
#include "ProfilerV2.hpp"
#include "logclassV1.h"
#include "NetProcess.h"

#pragma comment(lib, "ws2_32")

using namespace Net;
using namespace Core;
/////////////////////////////////////////////////////////
// Zone Session cpp
/////////////////////////////////////////////////////////


#pragma region stZoneSession

Net::stZoneSession::stZoneSession() : sock(INVALID_SOCKET), sessionId(0), refcount(SESSION_RELEASE_FLAG), recvQ(nullptr),
sendQ(nullptr), recvCntForMessage(0), zoneId(0), userPtr(nullptr), recvedPackets(4096),
recvOl(nullptr), sendOl(nullptr), isSending(0), isDisconnect(0), ip{}, port(0), sendPackets(nullptr), sendPacketsCnt(0)
{
	sendQ = new Core::CLockFreeQueue<Net::CPacket*>(SEND_Q_MAX_SIZE);
	recvOl = (myoverlapped*)malloc(sizeof(myoverlapped));
	sendOl = (myoverlapped*)malloc(sizeof(myoverlapped));
	if (recvOl == nullptr || sendOl == nullptr)
		__debugbreak();

	memset(recvOl, 0, sizeof(myoverlapped));
	memset(sendOl, 0, sizeof(myoverlapped));
	recvOl->isSend = false;
	sendOl->isSend = true;

	sendPackets = new Net::CPacket * [SERVER_SEND_WSABUF_MAX];
}

Net::stZoneSession::~stZoneSession()
{
	if (sendQ != nullptr)
		delete sendQ;
	if (recvOl != nullptr)
		free(recvOl);
	if (sendOl != nullptr)
		free(sendOl);

	if (sendPackets != nullptr)
		delete[] sendPackets;
}

//-------------------------------------------------------
// 스택에서 얻어온 index 바탕으로 소켓을 초기화
//-------------------------------------------------------
void Net::stZoneSession::Init(SOCKET _sock, int index, SOCKADDR_IN* caddr, uint64_t sid)
{
	_InterlockedIncrement(&refcount);
	sock = _sock;
	sid = ((sid << 20) | index);
	sessionId = sid;
	zoneId = 0;
	InetNtopW(AF_INET, &caddr->sin_addr, ip, 16);
	port = ntohs(caddr->sin_port);
	isDisconnect = 0;
	recvQ = CPACKET_ALLOC();
	recvQ->SetRecvBuffer();
	
	recvedPackets.ClearBuffer();
	userPtr = nullptr;
	//---------------------------------------------------
	// . 사용을 시작한다는 신호탄이자
	// . mfence의 역할
	// . 상위 1비트가 계속 1이면 다른곳에서는 나를 참조해도
	//   바로 참조를 풀음
	//---------------------------------------------------
	_InterlockedAnd((long*)&refcount, (~SESSION_RELEASE_FLAG));
}


//-------------------------------------------------------
// ReleaseSession에서만 호출!
// 소켓 초기화 후 스택에 반환해야 함, refcount == 0일때만!
//-------------------------------------------------------
void Net::stZoneSession::Clear()
{
	//---------------------------------------------------
	// ** 소켓 닫고 초기화
	// . Release Flag를 올린 스레드만 접근할 수 있다.
	// . 다만 iocount는 어디서든 올리고 내릴 수 있으므로
	// 초기화 하지 않는다.
	//---------------------------------------------------
	int retClose = closesocket(sock);
	if (retClose)
	{
		DWORD closeErr = GetLastError();
		Core::c_syslog::logging().LogEx(TAG_NET, closeErr, Core::c_syslog::en_ERROR, L"[sessionId: %016llx] 소켓 종료 오류(소켓 핸들: %016llx)", sessionId, sock);
	}
	sock = INVALID_SOCKET;
	sessionId = 0; // disconnect flag 알아서 0됨
	zoneId = 0;
	recvCntForMessage = 0;
	CPACKET_FREE(recvQ);
	// sendQ.Clear();
	//---------------------------------------------------
	// 센드큐는 직접 비우자..
	//---------------------------------------------------
	Net::CPacket* freePacket;
	while (sendQ->GetSize() > 0)
	{
		if (sendQ->Dequeue_Single(freePacket))
		{
			int retFree = CPACKET_FREE(freePacket);
			if (retFree)
			{
				Core::c_syslog::logging().Log(TAG_NET, Core::c_syslog::en_ERROR, L"패킷 반환중 오류, 패킷 클래스에 문제가 있습니다. (CPacket Free Error Code: %d)", retFree);
				__debugbreak();
			}
		}
	}

	//---------------------------------------------------
	// 센드 안한거 남았으면 정리
	//---------------------------------------------------
	for (int i = 0; i < sendPacketsCnt; i++)
	{
		int retFree = CPACKET_FREE(sendPackets[i]);
		if (retFree)
		{
			Core::c_syslog::logging().Log(TAG_NET, Core::c_syslog::en_ERROR, L"패킷 반환중 오류, 패킷 클래스에 문제가 있습니다. (CPacket Free Error Code: %d)", retFree);
			__debugbreak();
		}
	}
	sendPacketsCnt = 0;
	memset(recvOl, 0, sizeof(myoverlapped));
	memset(sendOl, 0, sizeof(myoverlapped));
	recvOl->isSend = false;
	sendOl->isSend = true;
	isSending = 0;
	memset(ip, 0, sizeof(ip));
	port = 0;

	//---------------------------------------------------
	// 리시브 패킷 큐도 정리
	//---------------------------------------------------
	recvedPackets.ClearBuffer();

	userPtr = nullptr;
	//---------------------------------------------------
	// . 여기는 mfence가 필요없다.
	// . 어짜피 Release Flag가 올라가있는 상태라 다른곳에서
	// 참조하더라도 아무것도 못한다
	//---------------------------------------------------
}

//-------------------------------------------------------
// refcount == 0이면 false반환
// . 얘는 세션이 생기면 올라가는 refcount를 내리는 역할이라서
// . 따로 참조하지 않고 들어가도되고 (always 참조상태)
// . 이 함수 내부에서 내리는 작업을 함
// 
// . 자신의 IO요청은 자기가 직접 필요시 해제
//-------------------------------------------------------
bool Net::stZoneSession::RecvPost()
{
	//---------------------------------------------------
	// 리시브는 일단 무조건! 항상 refcount가 올라간 상태
	// (처음 세션 생성할때 올려서 생성함)
	// 따라서 따로 검사하지 않음.
	// 
	// Recv에 대한 refcount를 내리는 주체도 RecvPost이므로
	// 걱정할 것이 없다. 
	// (만약 Recv완료통지에서 내렸으면 RecvPost로 들어오지 않는다)
	//---------------------------------------------------
	if (isDisconnect == 1)
	{
		unsigned long retNum = _InterlockedDecrement(&refcount);
		if (retNum == 0)
		{
			return false;
		}
		else
		{
			// TODO:
			if (retNum == 0xFFFF'FFFF)
				__debugbreak();
			return true;
		}
	}

	// 얼마나 받을지 모르니 포인터 이동은 받고 나서 하는 것으로 하자
	WSABUF wsabufs[1];
	wsabufs[0].buf = recvQ->GetWritePtr();
	wsabufs[0].len = recvQ->GetFreeSize();

	DWORD flags = 0;
	uint64_t curSessionId = sessionId;
	int retRecv = WSARecv(sock, wsabufs, 1, NULL, &flags, (WSAOVERLAPPED*)recvOl, NULL);
	if (retRecv == SOCKET_ERROR)
	{
		DWORD err = GetLastError();
		if (err != WSA_IO_PENDING)
		{	// IO 시도조차 무시된 경우
			_InterlockedExchange(&isDisconnect, 1);

			//----------------------------------------------
			// ** 중요한것을 빼먹음 **...
			//----------------------------------------------
			CancelIoEx((HANDLE)sock, &sendOl->ol);

			unsigned long retNum = _InterlockedDecrement(&refcount);
			if (retNum == 0)
			{
				Core::c_syslog::logging().Log(TAG_NET, Core::c_syslog::en_DEBUG, L"[sessionId: %016llx] Recv실패, refcount == 0, false반환", sessionId);
				return false;
			}
			else
			{
				Core::c_syslog::logging().Log(TAG_NET, Core::c_syslog::en_DEBUG, L"[sessionId: %016llx] Recv실패, refcount == 0 대기 (%d)", sessionId, retNum);
				if (retNum == 0xFFFF'FFFF)
					__debugbreak();
				return true;
			}
		}
		else
		{
			// Recv Pending
		}
	}
	else
	{
		// Recv 버퍼에 남은거 긁어옴
	}
	//---------------------------------------------------
	// 수신 종료 (Disconnect라면)
	// 
	// ** 중요 **
	// 여기부터는 이미 내 세션이 해제 되어있을 수 있음
	// (완료 통지 이후 refcount감소로)
	// 이제 여기서 다시 세션 검사가 필요
	// 그냥 검사하고 하면 안됨. 참조카운트 올리고 해야함
	//---------------------------------------------------
	if (isDisconnect == 1)
	{
		uint64_t retNum = _InterlockedIncrement(&refcount);
		if ((refcount & SESSION_RELEASE_FLAG) == SESSION_RELEASE_FLAG)
		{
			//-----------------------------------------------
			// 리시브는 내가 직접 내리고, 결과만 알림
			// 여기에서 ReleaseSession 호출이 안됨
			//-----------------------------------------------
			retNum = _InterlockedDecrement(&refcount);
			if (retNum == 0)
			{
				//---------------------------------------
				// 외부에 Release 요청
				//---------------------------------------
				return false;
			}
			else
			{
				if (retNum == 0xFFFF'FFFF)
					__debugbreak();
			}
			return true;
		}
		//-----------------------------------------------
		// 릴리즈 중이 아니였다면, 내가 내 세션 획득
		//-----------------------------------------------
		if (sessionId != curSessionId)
		{
			//-------------------------------------------
			// 세션이 바뀌었으니 외부에 _InterDec 요청
			//-------------------------------------------
			retNum = _InterlockedDecrement(&refcount);
			if (retNum == 0)
			{
				//---------------------------------------
				// 외부에 Release 요청
				//---------------------------------------
				return false;
			}
			else
			{
				if (retNum == 0xFFFF'FFFF)
					__debugbreak();
			}
			return true;
		}
		//-----------------------------------------------
		// 성공하지 않으면 이미 완료통지가 처리중이라는 뜻
		// 왜냐하면 내가 보낸다음 릴리즈를 막았고,
		// 내가 직접 내가보낸것을 취소하는 처리를 하기 때문
		//-----------------------------------------------
		CancelIoEx((HANDLE)sock, &recvOl->ol);
		//-----------------------------------------------
		// 일단 내부에서 올린 인터락 해제 해줄 주체가 필요
		//-----------------------------------------------
		retNum = _InterlockedDecrement(&refcount);
		if (retNum == 0)
		{
			//---------------------------------------
			// 외부에 Release 요청
			//---------------------------------------
			return false;
		}
		else
		{
			if (retNum == 0xFFFF'FFFF)
				__debugbreak();
		}
		return true;
	}

	return true;
}

//-------------------------------------------------------
	// . false: refcount를 내려야되면(완료 통지가 안와서)
	//                               + 내부에서 올려서 내려야함
	// . true: IOCP 완료통지에서 내려야한다!
	// 
	// . 자신의 IO요청은 자기가 직접 필요시 해제
	// 
	// . 반드시 외부에서 참조카운트가 올라간 상태로 진입
	// . 반드시 isSending = 1로 만들었을 경우만 진입
	// . => 반드시 외부에서 isDisconnect == 0 확인하고 진입해라
	// . false반환시 disconnect플래그 필요하면 올려둠
	//-------------------------------------------------------
bool stZoneSession::SendPost()
{
	PRO_BEGIN(L"NEW_GATHER_SENDPOST");
	WSABUF wsabufs[SERVER_SEND_WSABUF_MAX];
	int useSize;
	Net::CPacket* pPacket;
	int i;

	do
	{
		useSize = 0;
		pPacket = nullptr;
		//---------------------------------------------------
		// 일단 꺼내보고
		//---------------------------------------------------
		for (i = 0; i < SERVER_SEND_WSABUF_MAX; ++i)
		{
			bool ret = sendQ->Dequeue_Single(pPacket);
			if (ret)
			{
				CPACKET_UPDATE_TRACE(pPacket);
				wsabufs[i].buf = pPacket->GetBufferPtr();
				wsabufs[i].len = pPacket->GetDataSize();
				useSize += wsabufs[i].len;
				sendPackets[i] = pPacket;
			}
			else
			{
				// 큐가 비었다고 판단함
				break;
			}
		}

		if (i == 0) // Queue is Empty
		{
			//-----------------------------------------------
			// 보낼게 없음
			//-----------------------------------------------
			_InterlockedExchange(&isSending, 0);
			if (sendQ->GetSize() <= 0)
			{
				//------------------------------------------
				// 진짜 빈거, 외부에서 참조카운트 내려야함
				//------------------------------------------
				return false;
			}
			else
			{
				long trySend = _InterlockedExchange(&isSending, 1);
				if (trySend == 0)
				{
					//----------------------------------------
					// 내가 획득함
					//----------------------------------------
					if (isDisconnect == 1)
					{
						//------------------------------------
						// 끊어야 되는애라 보낼 이유 X
						//------------------------------------
						_InterlockedExchange(&isSending, 0);
						return false;
					}
				}
				else
				{
					//------------------------------------------
					// 다른애가 보내고있는거, 
					// 외부에서 참조카운트 내려야함
					//------------------------------------------
					return false;
				}
			}
		}
	} while (i == 0);	// i > 0 이면 나와야

	PRO_END(L"NEW_GATHER_SENDPOST");
	// i > 0 (보낼 개수가 0보다 큼)
	sendOl->sendbyte = useSize;
	sendPacketsCnt = i;
	//---------------------------------------------------
	// . 여기서 올리면 안됨. 안그러면 SendPost 시작 ~ 여기
	// 까지 refcount == 0일 수 있는 상태가 됨.
	// refcount == 0인상태로 다른 sendQ를 꺼낼 수 있음
	// . 즉 패킷이 섞일 수 있음!! (이미끊긴애 + 이제 보내야하는애)
	// => 사실 초기화 할 때 i도 초기화를 시키지만, 이렇게되면
	// sendPacketsCnt가 원자적이지 않아짐 (수정될 수 있음)
	// => 보내면 안되는것을 보내고, sendBytes도 이상해짐
	// refcount는 여기서는 관리하면 안되는게 맞네...
	//---------------------------------------------------
	uint64_t curSessionId = sessionId;	// WSASend보낸게 누구인지 미리 등록 (재사용 발생하면 다를 수 있음)

	//PRO_BEGIN(L"WSA_SEND");
	int retSend = WSASend(sock, wsabufs, i, NULL, 0, (WSAOVERLAPPED*)sendOl, NULL);
	if (retSend == SOCKET_ERROR)
	{
		DWORD err = GetLastError();
		//PRO_END(L"WSA_SEND");
		if (err != WSA_IO_PENDING)
		{
			//-------------------------------------------
			// 끊긴 연결이니 외부에서 참조카운트 감소해서
			// 0이라면 해제하면된다!
			// . LogEx는 특수한 연결종료일 경우 로그 남겨줌
			//-------------------------------------------
			Core::c_syslog::logging().LogEx(TAG_NET, err, Core::c_syslog::en_SYSTEM, L"[sessionId: %016llx] WSASend실패, 연결 종료된 세션", sessionId);

			//-------------------------------------------
			// 내가 보내려고 설정한거도 해제, 
			// 완료통지가 안오기 때문
			//-------------------------------------------
			for (int cnt = 0; cnt < sendPacketsCnt; cnt++)
			{
				int retFree = CPACKET_FREE(sendPackets[cnt]);
				if (retFree)
				{
					Core::c_syslog::logging().Log(TAG_NET, Core::c_syslog::en_ERROR, L"패킷 반환중 오류, 패킷 클래스에 문제가 있습니다. (CPacket Free Error Code: %d)", retFree);
					__debugbreak();
				}
			}
			sendPacketsCnt = 0;
			sendOl->sendbyte = 0;

			_InterlockedExchange(&isDisconnect, 1);

			//----------------------------------------------
			// ** 중요한것을 빼먹음 **...
			// . 이게 없으면 리시브는 여원히 걸릴 수 있음
			//----------------------------------------------
			CancelIoEx((HANDLE)sock, &recvOl->ol);

			_InterlockedExchange(&isSending, 0);
			
			return false;
		}
		else
		{
			// Send Pending
		}
	}
	else
	{
		// Send 송신 버퍼에다가
		//PRO_END(L"WSA_SEND");
	}

	//---------------------------------------------------
	// 송신 종료 (Disconnect라면)
	// 
	// ** 중요 **
	// 여기부터는 이미 내 세션이 해제 되어있을 수 있음
	// (완료 통지 이후 refcount감소로)
	// 이제 여기서 다시 세션 검사가 필요
	// 그냥 검사하고 하면 안됨. 참조카운트 올리고 해야함
	//---------------------------------------------------
	if (isDisconnect == 1)
	{
		uint64_t retNum = _InterlockedIncrement(&refcount);
		if ((refcount & SESSION_RELEASE_FLAG) == SESSION_RELEASE_FLAG)
		{
			//-----------------------------------------------
			// 외부에서 _InterlockedDecremnet유도
			// 여기에서 ReleaseSession 호출이 안됨
			//-----------------------------------------------
			
			return false;
		}
		//-----------------------------------------------
		// 릴리즈 중이 아니였다면, 내가 내 세션 획득
		//-----------------------------------------------
		if (sessionId != curSessionId)
		{
			//-------------------------------------------
			// 세션이 바뀌었으니 외부에 _InterDec 요청
			//-------------------------------------------
			
			return false;
		}
		//-----------------------------------------------
		// 성공하지 않으면 이미 완료통지가 처리중이라는 뜻
		//-----------------------------------------------
		CancelIoEx((HANDLE)sock, &sendOl->ol);
		//-----------------------------------------------
		// 일단 내부에서 올린 인터락 해제 해줄 주체가 필요
		//-----------------------------------------------
		
		return false;
	}

	return true;
}


#pragma endregion