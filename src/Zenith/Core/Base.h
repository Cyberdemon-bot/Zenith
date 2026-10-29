#pragma once

#include <memory>
#include <chrono>
#include <thread>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
	#ifdef _WIN64
		#define ZENITH_PLATFORM_WINDOWS
	#else
		#error "x86 Builds are not supported!"
	#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC == 1
		#define ZENITH_PLATFORM_MACOS
	#else
		#error "MacOS only!"
	#endif
#elif defined(__linux__)
	#define ZENITH_PLATFORM_LINUX
#else
	#error "Unknown platform!"
#endif

#ifdef ZENITH_PLATFORM_WINDOWS
	#define ZN_DEBUGBREAK() __debugbreak()
#elif defined(ZENITH_PLATFORM_LINUX) || defined(ZENITH_PLATFORM_MACOS)
	#include <signal.h>
	#define ZN_DEBUGBREAK() raise(SIGTRAP)
#else
	#define ZN_DEBUGBREAK()
#endif

#ifdef ZENITH_DEBUG
	#ifdef ZN_ENABLE_ASSERT
		#define ZN_ENABLE_ASSERTS
	#endif
#endif

#if defined(__clang__) && __has_cpp_attribute(clang::musttail)
    #define ZN_HAS_MUSTTAIL 1
#elif defined(__GNUC__) && (__GNUC__ >= 15) && __has_cpp_attribute(clang::musttail)
    #define ZN_HAS_MUSTTAIL 1
#else
    #define ZN_HAS_MUSTTAIL 0
#endif

namespace Zenith {

	#if ZN_HAS_MUSTTAIL
		using StepRet = void;
		#define ZN_NEXT() [[clang::musttail]] return STEP_TABLE[ip->slot()](state, ip, regs, consts)
		#define ZN_HALT() return
	#else
		using StepRet = const Instruction*; 
		#define ZN_NEXT() return ip
		#define ZN_HALT() return nullptr
	#endif
	

	constexpr size_t DEFAULT_STACK_SIZE = 4096;
    constexpr size_t MAX_CALL_FRAMES = 256;
	constexpr uint16_t MAX_Bx = 0xFFFFu;

	template<auto...>
    inline constexpr bool always_false_v = false;
}
