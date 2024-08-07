#pragma once
#include <chrono>
#include <iostream>
#include <algorithm>
#include "dspar/farm/farm.h"

#ifdef DSPARTIMINGS
double asSeconds(Duration::rep rawDuration) {
    Duration duration(rawDuration);
    return std::chrono::duration<double>(duration).count();
}
#endif

template <typename F, typename I>
void MeasureTime(F function, int parallelism, int rank, I *throughput)
{

    #ifdef DSPARTIMINGS
    bool timingsEnabled = true;
    #else
    bool timingsEnabled = false;
    #endif


    auto tstart = std::chrono::high_resolution_clock::now();
    function();

    I totalThroughput = (*throughput) == 0 && dspar::globals::isCollector && timingsEnabled?
        dspar::globals::collectorTimings.size() : *throughput;

    auto tend = std::chrono::high_resolution_clock::now();
    double TT = std::chrono::duration<double>(tend - tstart).count();
    double TR = ((double)(totalThroughput)) / TT;

    if (dspar::globals::isCollector)
    {
#ifdef DSPARTIMINGS
        std::sort(dspar::globals::collectorTimings.begin(), dspar::globals::collectorTimings.end(), [](dspar::Timings &t1, dspar::Timings &t2) {
            return t1.totalTime < t2.totalTime;
        });

/*
	Duration::rep totalComputeTime;
	Duration::rep totalIoTime;
	Duration::rep totalTime;
*/

        auto procTimeMin = dspar::globals::collectorTimings[0].totalTime;
        auto procTime25 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.25)].totalTime;
        auto procTimeMedian = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.5)].totalTime;
        auto procTime75 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.75)].totalTime;
        auto procTime95 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.95)].totalTime;
        auto procTimeMax = dspar::globals::collectorTimings[dspar::globals::collectorTimings.size() - 1].totalTime;

        double totalProc = 0;
        for (auto c: dspar::globals::collectorTimings) {
            totalProc += c.totalTime;
        }

        
        std::sort(dspar::globals::collectorTimings.begin(), dspar::globals::collectorTimings.end(), [](dspar::Timings &t1, dspar::Timings &t2) {
            return t1.totalIoTime < t2.totalIoTime;
        });

        auto ioTimeMin = dspar::globals::collectorTimings[0].totalIoTime;
        auto ioTime25 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.25)].totalIoTime;
        auto ioTimeMedian = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.5)].totalIoTime;
        auto ioTime75 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.75)].totalIoTime;
        auto ioTime95 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.95)].totalIoTime;
        auto ioTimeMax = dspar::globals::collectorTimings[dspar::globals::collectorTimings.size() - 1].totalIoTime;

        double totalIo = 0;
        for (auto c: dspar::globals::collectorTimings) {
            totalIo += c.totalIoTime;
        }


        std::sort(dspar::globals::collectorTimings.begin(), dspar::globals::collectorTimings.end(), [](dspar::Timings &t1, dspar::Timings &t2) {
            return t1.totalComputeTime < t2.totalComputeTime;
        });

        auto computeTimeMin = dspar::globals::collectorTimings[0].totalComputeTime;
        auto computeTime25 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.25)].totalComputeTime;
        auto computeTimeMedian = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.5)].totalComputeTime;
        auto computeTime75 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.75)].totalComputeTime;
        auto computeTime95 = dspar::globals::collectorTimings[(int)((float)dspar::globals::collectorTimings.size() * 0.95)].totalComputeTime;
        auto computeTimeMax = dspar::globals::collectorTimings[dspar::globals::collectorTimings.size() - 1].totalComputeTime;

        double totalCompute = 0;
        for (auto c: dspar::globals::collectorTimings) {
            totalCompute += c.totalComputeTime;
        }
        
#endif

        std::cout << std::fixed << parallelism << "\t"
                  << TT << "\t"
                  << TR
#ifdef DSPARTIMINGS
                  << "\t"
                  << "procMin: " << asSeconds(procTimeMin) << "\t"
                  << "proc25: " << asSeconds(procTime25) << "\t"
                  << "proc50: " << asSeconds(procTimeMedian) << "\t"
                  << "proc75: " << asSeconds(procTime75) << "\t"
                  << "proc95: " << asSeconds(procTime95) << "\t"
                  << "procMax: " << asSeconds(procTimeMax) << "\t"
                  << "procSum: " << asSeconds(totalProc) << "\t"

                  << "ioMin: " << asSeconds(ioTimeMin) << "\t"
                  << "io25: " << asSeconds(ioTime25) << "\t"
                  << "io50: " << asSeconds(ioTimeMedian) << "\t"
                  << "io75: " << asSeconds(ioTime75) << "\t"
                  << "io95: " << asSeconds(ioTime95) << "\t"
                  << "ioMax: " << asSeconds(ioTimeMax) << "\t"
                  << "ioSum: " << asSeconds(totalIo) << "\t"

                  << "computeMin: " << asSeconds(computeTimeMin) << "\t"
                  << "compute25: " << asSeconds(computeTime25) << "\t"
                  << "compute50: " << asSeconds(computeTimeMedian) << "\t"
                  << "compute75: " << asSeconds(computeTime75) << "\t"
                  << "compute95: " << asSeconds(computeTime95) << "\t"
                  << "computeMax: " << asSeconds(computeTimeMax) << "\t"
                  << "computeSum: " << asSeconds(totalCompute)
#endif
                  << std::endl;
    }


}
