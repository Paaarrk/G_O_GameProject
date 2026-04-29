#ifndef __DB_REQUEST_H__
#define __DB_REQUEST_H__

#include "DBThreadPool.h"
#include <vector>
#include <string>
#include <tuple>

struct stDBReqHeader
{
	void (*excutefunc)(void*, CTlsMySqlConnector&);
	void (*destructer)(void*);
};

template <typename F>
struct stDBReq
{
	stDBReqHeader header;
	F lambda;

	stDBReq(F&& f) : lambda(std::forward<F>(f))
	{
		header.excutefunc = [](void* thisptr, CTlsMySqlConnector& conn) {
			static_cast<stDBReq<F>*>(thisptr)->lambda(conn);
		});
		header.destructer = [](void* thisptr) {
			static_cast<stDBReq<F>*>(thisptr)->~stDBReq<F>();
		};
	}
	~stDBReq() {}
};

enum ERequestType
{
	ASYNC_GET_ACCOUNT_INFO,
	ASYNC_GET_CHARACTER_INFO,
};

struct stDBResponse
{
	uint64_t sessionId;
	int reqtype;
};
struct stAccountInfo : public stDBResponse
{
	int64_t accoutno;
	char userId[20];
	char usernick[20];
	int32_t status;
};

struct stCharacterInfo : public stDBResponse
{
	int64_t accountno;
	int32_t charactertype;
	float posx;
	float posy;
	int32_t tilex;
	int32_t tiley;
	int32_t rotation;
	int32_t cristal;
	int32_t hp;
	int64_t exp;
	int32_t level;
	int32_t die;
};

class CDBRequest
{
public:
	// ipv4 string
	static std::vector<std::string> Sync_GetWhiteIpList();
	// type, cnt pair
	static std::vector<std::pair<int, int>> Sync_GetCrystalData();
	// player data (damage, hp, recovery_hp)
	static std::vector<std::tuple<int, int, int>> Sync_GetPlayerBaseData();
	// type, hp, damage
	static std::vector<std::tuple<int, int, int>> Sync_GetMonsterBaseData();

	template <typename... Args>
	static void Async_RequestSingleQuery(uint64_t sessionId, int reqtype, const wchar_t* format, Args&&... args);
	
private:

};


#endif