#include "DBRequest.h"


int main()
{
	
	std::vector<std::string> v = CDBRequest::Sync_GetWhiteIpList();
	std::vector<std::pair<int, int>> v2 = CDBRequest::Sync_GetCrystalData();
	std::vector<std::tuple<int, int, int>> v3 = CDBRequest::Sync_GetPlayerBaseData();
	std::vector<std::tuple<int, int, int>> v4 = CDBRequest::Sync_GetMonsterBaseData();
	for (std::string& s : v)
	{
		printf("%s\n", s.c_str());
	}
	for (const auto& [no, cnt] : v2)
	{
		printf("[type(%d): %d] \n", no, cnt);
	}
	for (const auto& [damage, hp, recovery_hp] : v3)
	{
		printf("[damage(%d) | hp(%d) | recovery_hp(%d)] \n", damage, hp, recovery_hp);
	}
	for (const auto& [type, hp, damage] : v4)
	{
		printf("[type(%d) | hp(%d) | damage(%d)] \n", type, hp, damage);
	}

	CDBRequest::Async_RequestQuery(nullptr);
}
