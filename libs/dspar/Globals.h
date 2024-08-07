#pragma once
#include <vector>
#include "Timings.h"


namespace dspar
{
    namespace globals
    {
        std::vector<Timings> collectorTimings;
        bool isCollector = false;
        int collectorRank = -1;
    
        bool isEmitter = false;
        int emitterRank = -1;

        int argc = -1;
        char** argv = NULL;
    } // namespace globals

};

 // namespace dspar