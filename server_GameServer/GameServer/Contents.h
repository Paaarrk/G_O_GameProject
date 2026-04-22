#pragma once
#include <chrono>

using SteadyClock = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;

inline static int64_t GetDeltaTimeMs(const TimePoint& end, const TimePoint& start) noexcept
{
	return duration_cast<ms>(end - start).count();
}

inline const wchar_t* TAG_LOBBY =		L"Lobby";
inline const wchar_t* TAG_GAME =		L"Game";
inline const wchar_t* TAG_MESSAGE =		L"Message";
inline const char*	 CONFIG_FILE_PATH = "..\\Config\\Config.cnf";

inline constexpr int SESSION_KEY_LEN = 64;
inline constexpr int IPV4_LEN = 16;

enum en_POOL_KEYS
{
	POOLKEY_PLAYER = 0xFFFF'F000,
};

enum en_CONTENTS
{
	GMAE_NICKNAME_LEN = 20,

	CONTENTS_ID_LOBBY = 1,
	CONTENTS_ID_GAME = 2,

	MONITORING_TICK = 1000,

	TIME_OUT_MS_LOBBY = 20000,
	TIME_OUT_MS_GAME = 40000,
};