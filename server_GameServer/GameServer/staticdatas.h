#ifndef __STATIC_DATAS_H__
#define __STATIC_DATAS_H__

#include "Contents.h"

#include <vector>
#include <unordered_map>
#include <string>


class CStaticDatas
{
public:
	static bool Init() { return s_datas.Load(); }
	static bool Is_White(const wchar_t* wip) { return s_datas.IsWhite(wip); }
	static int32_t Crystal_Cnt(ECrystalTypes TYPE) { return s_datas.CrystalCnt(TYPE); }
	static int32_t Player_Damage() { return s_datas.PlayerDamage(); }
	static int32_t Player_Hp() { return s_datas.PlayerHp(); }
	static int32_t Player_RecoverHp() { return s_datas.PlayerRecoverHp(); }
	// -1, -1리턴 (실패시)
	std::pair<int32_t, int32_t> Monster_Data(EMonsterTypes TYPE) const { return s_datas.MonsterData(TYPE); }

private:
	CStaticDatas(){}
	~CStaticDatas() = default;

	bool Load();
	bool IsWhite(const wchar_t* wip) const
	{
		for (const FixedWString<IPV4_LEN>& ip : _whitelist)
		{
			if (wcscmp(ip.data, wip) == 0)
				return true;
		}
		return false;
	}
	int32_t CrystalCnt(ECrystalTypes TYPE) const 
	{
		auto it = _crystalData.find(TYPE);
		if (it == _crystalData.end()) return 0;
		
		return it->second;
	}
	int32_t PlayerDamage() const { return _playerDamage; }
	int32_t PlayerHp() const { return _playerHp; }
	int32_t PlayerRecoverHp() const { return _playerRecoverHp; }
	std::pair<int32_t, int32_t> MonsterData(EMonsterTypes TYPE) const
	{
		auto it = _monsterData.find(TYPE);
		if (it == _monsterData.end()) return { -1, -1 };
		return it->second;
	}

	std::vector<FixedWString<IPV4_LEN>> _whitelist;
	// key: type(32), data: drop cnt(32)
	std::unordered_map<int32_t, int32_t> _crystalData;
	int32_t _playerDamage = 0;
	int32_t _playerHp = 0;
	int32_t _playerRecoverHp = 0;

	// key: type(32), data: {hp(32), damage(32)}
	std::unordered_map<int32_t, std::pair<int32_t, int32_t>> _monsterData;

	static CStaticDatas s_datas;
};

#endif