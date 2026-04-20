#pragma once
#include <windows.h>
#include <wchar.h>
#include "logclassV1.h"

#define PROFILE_PQCS_RATE
#ifdef PROFILE_PQCS_RATE
#define Increase_Send_Total()	CProfilePQCSRate::Inst().CProfilePQCSRate::IncreaseSendTotal()
#define Increase_Pqcs_Total()	CProfilePQCSRate::Inst().CProfilePQCSRate::IncreasePqcsTotal()
#define Print_Result()			CProfilePQCSRate::Inst().CProfilePQCSRate::PrintResult()
#else
#define Increase_Send_Total()	
#define Increase_Pqcs_total()
#define Print_Result()
#endif

class CProfilePQCSRate
{
public:
	static CProfilePQCSRate& Inst()
	{
		static CProfilePQCSRate obj;
		return obj;
	}
	void IncreaseSendTotal()
	{
		_InterlockedIncrement(&sendtotal);
	}
	void IncreasePqcsTotal()
	{
		_InterlockedIncrement(&pqcstotal);
	}
	void PrintResult() const
	{
		wprintf_s(L"[pqcstotal :%d / sendtotal: %d] rate: %lf", pqcstotal, sendtotal, (double)pqcstotal / sendtotal);
		Core::c_syslog::logging().Log(L"PQCS RATE", Core::c_syslog::en_SYSTEM, L"[pqcstotal :%d / sendtotal: %d] rate: %lf", pqcstotal, sendtotal, (double)pqcstotal / sendtotal);
	}
private:
	long sendtotal = 0;
	long pqcstotal = 0;
};