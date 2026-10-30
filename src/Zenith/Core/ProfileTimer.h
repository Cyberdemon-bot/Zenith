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
		if (duration > minTime) *result = duration;
	}
};

#define ZN_PROFILE_SCOPE(res) ProfileTimer timer##__LINE__(res)