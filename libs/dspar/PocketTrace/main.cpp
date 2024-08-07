#include <iostream>
#define TRACE_PROFILER
#include "TraceProfiler.h"
#include "TraceProfiler.cpp"

int main() {
    std::cout<<"start "<<std::endl;
    atexit(TraceShutdown);
    TraceInit("trace_test");
    TRTHREADPROC("MAINTHREAD");
    std::cout<<"trace init "<<std::endl;
    TRACE();
    for (int i=0; i< 100; i++) {
        {
            TRBLOCK("wait");
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        {
            TRBLOCK("print");
            std::cout<<"num "<<i<<std::endl;
        }
    }
}