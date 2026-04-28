#pragma once
#include <chrono>
#include <type_traits>
#include <concepts>

using SteadyClock = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;

inline static int64_t GetDeltaTimeMs(const TimePoint& end, const TimePoint& start) noexcept
{
	return duration_cast<ms>(end - start).count();
}
inline static int32_t GetDeltaTimeMs(unsigned long end, unsigned long start)
{
	return static_cast<int32_t>(end - start);
}

inline const wchar_t*	TAG_CONTENTS =		L"Contents";
inline const wchar_t*	TAG_LOBBY =			L"Lobby";
inline const wchar_t*	TAG_GAME =			L"Game";
inline const wchar_t*	TAG_MESSAGE =		L"Message";
inline const wchar_t*	TAG_TO_CHAT =		L"ClientToChat";
inline const char*		CONFIG_FILE_PATH =	"..\\Config\\Config.cnf";

template<typename T>
concept CStyleBaseTypes = requires {
	typename std::remove_cvref_t<T>;
} 
&& (!std::is_pointer_v<std::remove_cvref_t<T>>)
&& (std::is_fundamental_v<std::remove_all_extents_t<std::remove_cvref_t<T>>>);

template<CStyleBaseTypes... Args>
constexpr inline int32_t SizeOf(Args&&... args)
{
	return (0 + ... + static_cast<int32_t>(sizeof(args)));
}

enum en_POOL_KEYS
{
	POOLKEY_PLAYER = 0xFFFF'F000,
};

enum en_CONTENTS
{
	SIGNAL_OFF = 0,
	SIGNAL_ON = 1,
	SESSION_KEY_LEN = 64,
	IPV4_LEN = 16,
	MESSAGE_HEADER_LEN = 2,

	GAME_NICKNAME_LEN = 20,

	CONTENTS_ID_LOBBY = 1,
	CONTENTS_ID_GAME = 2,

	MONITORING_TICK = 1000,

	TIME_OUT_MS_LOBBY = 20000,
	TIME_OUT_MS_GAME = 40000,

	LOBBY_RINGBUFFER_SIZE = 5000,
};