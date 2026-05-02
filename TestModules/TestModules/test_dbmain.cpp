#include "DBRequest.h"
#include "DBThreadPool.h"

int main()
{
	mysql_library_init(0, NULL, NULL);
	

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

	int requestNum = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetRequestNum();
	int queuesize = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetQueueSize<POOL_USE_LOBBY>();
	printf("requestNum: %d, queuesize: %d \n", requestNum, queuesize);


	IAsyncRequest* info = new CAsync_GetAccountInfo(POOL_USE_LOBBY, 1, 2, [](const GetAccountInfoResType& res){
		const auto& [accountno, id, nick, status] = res;
		if (accountno == 0)
		{
			printf("account not found\n");
			return;
		}

		printf("(accountno: %lld), (id: %s), (nick: %s), (status: %d)\n",
			accountno, id.name, nick.name, status);
	});
	CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(info);
	printf("enque fin \n");

	requestNum = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetRequestNum();
	queuesize = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetQueueSize<POOL_USE_LOBBY>();
	printf("requestNum: %d, queuesize: %d \n", requestNum, queuesize);

	info = new CAsync_GetCharacterInfo(POOL_USE_LOBBY, 1, 100, [](const GetCharacterInfoResType& res) {
		const auto& [accountno, charactertype, posx, posy, tilex, tiley, rotation, cristal, hp, exp, level, die] = res;
		if (accountno == 0)
		{
			printf("character not found, please make new character\n");
			return;
		}

		printf("(accountno: %lld, charactertype: %d, posx: %f, posy: %f, tilex: %d, tiley: %d, rotation: %d, cristal: %d, hp: %d, exp: %lld, level: %d, die: %d) \n",
			accountno, charactertype, posx, posy, tilex, tiley, rotation, cristal, hp, exp, level, die);
		});
	CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().RequestQuery(info);
	printf("enque fin \n");


	requestNum = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetRequestNum();
	queuesize = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetQueueSize<POOL_USE_LOBBY>();
	printf("requestNum: %d, queuesize: %d \n", requestNum, queuesize);

	IAsyncRequest* ptr;
	while ((ptr = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetResponse<POOL_USE_LOBBY>()) == nullptr) { _mm_pause(); }
	ptr->ResponseProcess();
	delete ptr;

	requestNum = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetRequestNum();
	queuesize = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetQueueSize<POOL_USE_LOBBY>();
	printf("requestNum: %d, queuesize: %d \n", requestNum, queuesize);

	while ((ptr = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetResponse<POOL_USE_LOBBY>()) == nullptr) { _mm_pause(); }
	ptr->ResponseProcess();
	delete ptr;
	
	requestNum = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetRequestNum();
	queuesize = CDBThreadPool<POOL_USE_COUNT>::GetDBThreadPool().GetQueueSize<POOL_USE_LOBBY>();
	printf("requestNum: %d, queuesize: %d \n", requestNum, queuesize);

	return 0;
}
