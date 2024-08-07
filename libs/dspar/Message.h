#pragma once

#include <chrono>

//#define DSPARTIMINGS

#ifdef DSPARTIMINGS
using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;
#endif

namespace dspar
{
	struct MessageHeader
	{
		int sender;
		int target;
		int type;
#ifdef DSPARTIMINGS
		//accumulates all .Process() times for this msg id
		Duration::rep totalComputeTime;
		Duration::rep ts;
#endif
		uint64_t id;
	};

} // namespace dspar