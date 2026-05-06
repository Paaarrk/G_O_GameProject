#include "staticdatas.h"
#include "DBRequest.h"

CStaticDatas CStaticDatas::s_datas;

bool CStaticDatas::Load()
{
	/* White IP List */
	std::vector<std::string> v = CDBRequest::Sync_GetWhiteIpList();
	int n = static_cast<int>(v.size());
	if (n == 0) return false;

	_whitelist.resize(n);
	for (int i = 0; i < n; i++)
	{
		std::string& s = v[i];
		FixedWString<IPV4_LEN>& ws = _whitelist[i];

		MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, ws.data, IPV4_LEN);
	}

	/* Crystal Data */
	auto v_cd = CDBRequest::Sync_GetCrystalData();
	if (v_cd.size() == 0) return false;
	for (auto& [type, dropcnt] : v_cd)
	{
		_crystalData[type] = dropcnt;
	}

	/* Player Data */
	auto v_p = CDBRequest::Sync_GetPlayerBaseData();
	if (v_p.size() != 1) return false;

	auto& [damage, hp, recovery_hp] = v_p[0];
	_playerDamage = damage;
	_playerHp = hp;
	_playerRecoverHp = recovery_hp;

	/* Monster Data */
	auto v_m = CDBRequest::Sync_GetMonsterBaseData();
	if (v_m.size() == 0) return false;
	
	_monsterData.reserve(v_m.size());
	for (auto& [type, hp, damage] : v_m)
	{
		_monsterData[type] = { hp, damage };
	}

	return true;
}


