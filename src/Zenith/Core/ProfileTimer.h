#pragma once

#include <chrono>

struct ProfileTimer 
{
    float* result;
	float minTime;
	std::chrono::steady_clock::time_point start;

	ProfileTimer(float* res, float mt = 0.0f) : result(res), minTime(mt), start(std::chrono::steady_clock::now()) {}
	~ProfileTimer()
	{
		auto end = std::chrono::steady_clock::now();
		float duration = std::chrono::duration<float, std::milli>(end - start).count();
		if (result && duration > minTime) *result = duration;
	}
};

#define ZN_CONCAT_IMPL(a, b) a##b
#define ZN_CONCAT(a, b) ZN_CONCAT_IMPL(a, b)
#define ZN_PROFILE_SCOPE(res) ProfileTimer ZN_CONCAT(timer, __LINE__)(res)