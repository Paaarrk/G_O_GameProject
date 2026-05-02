#include "DBRequest.h"
#include "DBThreadPool.h"
#include "DBWriter.h"

int main()
{
	mysql_library_init(0, NULL, NULL);
	

	CDBWriterThread writer;

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


	IDBWriterRequest* wreq = new CAsync_WriteRequest(L"INSERT INTO `gamedb`.`character` values (%lld, %d, %f, %f, %d, %d, %d, %d, %d, %lld, %d, %d)", [](int ret) {
		printf("Reqeust fin!!\n"); }, (int64_t)102, 2, 4.0f, 4.0f, 0, 0, 125, 200, 19, (int64_t)0, 999, 0);
	//writer.Request(wreq);

	// const wchar_t* login = L"Login";
	FixedWString string(L"Login");

	wreq = new CAsync_WriteRequest(L"INSERT INTO `logdb`.`gamelog_template` (accountno, servername, type, code, param1) values(%lld, '%s', %d, %d, %d)",
		[](int ret) {printf("Request fin!!: %d \n", ret); }, (int64_t)2345, std::move(string), 100, 101,  1);
	//writer.Request(wreq);

	FixedWString stringgame(L"Game");
	wreq = new CAsync_WriteRequest(L"BEGIN; \
UPDATE `gamedb`.`character` SET `cristal` = %d WHERE `accountno` = %lld ; \
INSERT INTO `logdb`.`gamelog_template` (accountno, servername, type, code, param1, param2) \
SELECT %lld, '%s', %d, %d, %d, %d FROM DUAL WHERE ROW_COUNT() > 0 ; SELECT ROW_COUNT(); COMMIT;",
		[](int ret) {printf("Request fin!!: %d \n", ret); }, 40, (int64_t)101,
		(int64_t)100, std::move(stringgame), 4, 41, 30, 40);
	writer.Request(wreq);

	return 0;
}
