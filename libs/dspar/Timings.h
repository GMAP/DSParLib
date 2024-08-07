#pragma once

#include <chrono>

namespace dspar
{

    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    struct Timings
    {
        Duration::rep totalComputeTime;
        Duration::rep totalIoTime;
        Duration::rep totalTime;
    };
} // namespace dspar