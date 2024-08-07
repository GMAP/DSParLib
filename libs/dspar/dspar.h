#pragma once

//#define ENABLE_SERDE_LOG
#define ENABLE_NODE_LOG
//#define ASYNC_DEMAND

#ifdef WINDOWS
#include <mpi.h>
#endif

#ifdef MACOSX
#include "mpi.h"
#endif

#ifndef MACOSX
#ifndef WINDOWS
#include "mpi.h"
#endif
#endif

#include <queue>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <iterator>
#include <math.h>
#include <iostream>
#include <memory.h>
//#define TRACE_PROFILER
#include "PocketTrace/TraceProfiler.h"
#include "PocketTrace/TraceProfiler.cpp"

#if _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#ifdef DSPARDEBUG
#define LOG_DEBUG(x) std::cout << "DSpar DEBUG " << __PRETTY_FUNCTION__ << ": " << x << std::endl
#define DSPAR_DEBUG(x) std::cout << "Rank " << GetMyRank() << ": DSpar DEBUG " << __PRETTY_FUNCTION__ << ": " << x << std::endl
#else
#define LOG_DEBUG(x)
#define DSPAR_DEBUG(x)
#endif
#define LOG_ERROR(x) std::cerr << "DSpar ERROR " << __PRETTY_FUNCTION__ << ": " << x << std::endl
#define LOG_ERROR_AND_THROW(x)                                                    \
	std::cerr << "DSpar ERROR " << __PRETTY_FUNCTION__ << ": " << x << std::endl; \
	throw std::runtime_error(x)

#define TEST_DEBUG(x) std::cout << "TEST " << __PRETTY_FUNCTION__ << ": " << x << std::endl
#define TEST_ERROR(x, err) std::cerr << "\033[14;31m" \
									 << "TEST " << x << " FAILED: " << err << "\033[0m" << std::endl
#define TEST_STARTED() std::cout << "\033[14;33m" \
								 << "TEST " << __PRETTY_FUNCTION__ << " STARTED \033[0m" << std::endl
#define TEST_PASSED(x) std::cout << "\033[14;32m" \
								 << "TEST " << x << " PASSED  \033[0m" << std::endl

#ifdef ENABLE_SERDE_LOG
#define SERDE_DEBUG(x) LOG_DEBUG(x)
#define SERDE_ERROR(x) LOG_ERROR(x)
#else
#define SERDE_DEBUG(x)
#define SERDE_ERROR(x)
#endif // ENABLE_SERDE_LOG

const int MPI_DSPAR_STOP = 0;
const int MPI_DSPAR_MESSAGE_BOUNDARY = 1;
const int MPI_DSPAR_STREAM_MESSAGE = 2;
const int MPI_DSPAR_DEMAND = 3;

const int MESSAGE_TYPE = 0;
const int STOP_TYPE = 1;
const int NO_MORE_DEMAND_TYPE = 1;

#include "Message.h"
#include "CircularVector.h"
#include "DSParNodeConfiguration.h"
#include "Timings.h"
#include "Globals.h"
#include "DemandSignal.h"
#include "MPIUtils.h"
#include "MPIReceiver.h"
#include "MPISender.h"
#include "DSparLifecycle.h"
#include "utils/Timer.h"
