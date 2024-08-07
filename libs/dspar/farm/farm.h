#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <functional>
#include <algorithm>
#include "dspar/dspar.h"
#include <map>
#include "dspar/wrappers.h"
#include "dspar/SenderReceiver.h"
#include "dspar/Pipeline.h"
#include "dspar/DSparNode.h"

#define ForBetween(start, end) for (int it = start; it < end; it++)
#define For(n) ForBetween(0, n)
#define IsLast(i) i == end - 1

namespace dspar
{

	template <typename EmitterInput, typename EmitterOutput,
			  typename WorkerOutput,
			  typename CollectorOutput>
	class FarmPattern : public AbstractPipelineElement
	{
	private:
		SenderReceiver<EmitterInput> &worldToEmitter;

		Wrapper<EmitterInput, EmitterOutput> &emitter;
		SenderReceiver<EmitterOutput> &emitterToWorkers;

		Wrapper<EmitterOutput, WorkerOutput> &worker;
		int workerReplicas = 1;
		SenderReceiver<WorkerOutput> &workerToCollector;

		Wrapper<WorkerOutput, CollectorOutput> &collector;
		SenderReceiver<CollectorOutput> &collectorToWorld;
		bool collectorIsOrdered = false;
		bool useOnDemandScheduling = false;

	public:
		FarmPattern(
			SenderReceiver<EmitterInput> &_worldToEmitter,

			Wrapper<EmitterInput, EmitterOutput> &_emitter,
			SenderReceiver<EmitterOutput> &_emitterToWorkers,

			Wrapper<EmitterOutput, WorkerOutput> &_worker,
			SenderReceiver<WorkerOutput> &_workerToCollector,

			Wrapper<WorkerOutput, CollectorOutput> &_collector,
			SenderReceiver<CollectorOutput> &_collectorToWorld) : worldToEmitter(_worldToEmitter),
																  emitter(_emitter),
																  emitterToWorkers(_emitterToWorkers),
																  worker(_worker),
																  workerToCollector(_workerToCollector),
																  collector(_collector),
																  collectorToWorld(_collectorToWorld)
		{

#ifdef TRACE_PROFILER
			std::cout << "Trace Profiling enabled" << std::endl;
			atexit(TraceShutdown);
			TraceInit("tracing");
#endif
		};

		int Start(MPI_Comm comm, int startingRank = 0)
		{
			return this->Start(comm, startingRank, std::vector<int>(), std::vector<int>());
		};

		int Start(MPI_Comm comm, int startingRank,
				  std::vector<int> inputRanks,
				  std::vector<int> outputRanks) override
		{

			//start serializers
			worldToEmitter.Start();
			emitterToWorkers.Start();
			workerToCollector.Start();
			collectorToWorld.Start();

			dspar::MPIUtils utils;
			int myRank = utils.GetMyRank(comm);

#ifdef TRACE_PROFILER
			std::stringstream ss;
			ss << "Rank" << myRank;
			const char *threadname = ss.str().c_str();
			TRTHREADPROC(threadname);
#endif

			TRACE();

			int totalNumberOfProcessesNeeded = GetTotalNumberOfProcessesNeeded();

			if (myRank < startingRank)
				return totalNumberOfProcessesNeeded + startingRank;

			auto emitterRanks = GetEmitterRanks(startingRank);
			auto workerRanks = GetWorkersRanks(startingRank);
			auto collectorRanks = GetCollectorRanks(startingRank);

			auto rankIsEmitter = std::find(emitterRanks.begin(), emitterRanks.end(), myRank) != emitterRanks.end();
			auto rankIsWorker = std::find(workerRanks.begin(), workerRanks.end(), myRank) != workerRanks.end();
			auto rankIsCollector = std::find(collectorRanks.begin(), collectorRanks.end(), myRank) != collectorRanks.end();

			DSParNodeConfiguration nodeConfig;

			if (rankIsEmitter)
			{
				TRBLOCK("Emitter");

				if (emitterRanks.size() > 1)
				{
					LOG_ERROR_AND_THROW("Cannot run more than 1 emitter node");
				}

				if (useOnDemandScheduling)
				{
					nodeConfig.WaitForDemandDownstream = true;
					nodeConfig.AskForDemandUpstream = false;
				}

				DSparNode<EmitterInput, EmitterOutput> farmEmitter(
					emitter, worldToEmitter,
					emitterToWorkers, workerRanks,
					inputRanks, nodeConfig);
				farmEmitter.StartNode(comm);
			}
			else if (rankIsWorker)
			{
				if (useOnDemandScheduling)
				{
					nodeConfig.WaitForDemandDownstream = false;
					nodeConfig.AskForDemandUpstream = true;
				}

				TRBLOCK("Worker");

				DSparNode<EmitterOutput, WorkerOutput> farmWorkerNode(
					worker, emitterToWorkers,
					workerToCollector,
					collectorRanks,
					emitterRanks, nodeConfig);

				farmWorkerNode.StartNode(comm);
			}
			else if (rankIsCollector)
			{
				if (useOnDemandScheduling)
				{
					nodeConfig.WaitForDemandDownstream = false;
					nodeConfig.AskForDemandUpstream = false;
				}
				else
				{
					nodeConfig.WaitForDemandDownstream = false;
					nodeConfig.AskForDemandUpstream = false;
				}

				if (collectorIsOrdered)
				{
					nodeConfig.Ordered = true;
				}

				TRBLOCK("Collector");
				DSparNode<WorkerOutput, CollectorOutput> farmCollectorNode(
					collector, workerToCollector,
					collectorToWorld, outputRanks,
					workerRanks, nodeConfig);
				farmCollectorNode.StartNode(comm);
			}

			return totalNumberOfProcessesNeeded + startingRank;
		};

		void SetCollectorIsOrdered(bool _collectorIsOrdered)
		{
			this->collectorIsOrdered = _collectorIsOrdered;
		}

		void SetOnDemandScheduling(bool _onDemandScheduling)
		{
			this->useOnDemandScheduling = _onDemandScheduling;
		}

		void SetWorkerReplicas(int _workerReplicas)
		{
			this->workerReplicas = _workerReplicas;
		}

		int GetTotalNumberOfProcessesNeeded() override
		{
			return 1 + workerReplicas + 1;
		};

		std::vector<int> GetEmitterRanks(int startingRank)
		{
			std::vector<int> ranks;
			ranks.push_back(startingRank);
			return ranks;
		};

		std::vector<int> GetCollectorRanks(int startingRank)
		{
			std::vector<int> ranks;
			ranks.push_back(1 + startingRank);
			return ranks;
		};

		std::vector<int> GetWorkersRanks(int startingRank)
		{
			std::vector<int> ranks;
			for (int i = 1 + 1; i < 2 + workerReplicas; i++)
			{
				ranks.push_back(i + startingRank);
			}
			return ranks;
		};

		std::vector<int> GetInputOffsetRanks() override
		{
			return GetEmitterRanks(0);
		};

		std::vector<int> GetOutputOffsetRanks() override
		{
			return GetCollectorRanks(0);
		};

		void PrintGraph(int startingRank, int indentationLevel) override
		{

			std::string indent = "";

			For(indentationLevel)
			{
				indent += "    ";
			}

			std::cout << indent << "Farm: {" << std::endl;
			std::cout << indent << "    Emitters: ";
			for (auto rank : GetEmitterRanks(startingRank))
			{
				std::cout << rank;
				std::cout << ", ";
			}
			std::cout << std::endl;

			std::cout << indent << "    Collectors: ";
			for (auto rank : GetCollectorRanks(startingRank))
			{
				std::cout << rank;
				std::cout << ", ";
			}
			std::cout << std::endl;

			std::cout << indent << "    Workers: ";
			for (auto rank : GetWorkersRanks(startingRank))
			{
				std::cout << rank;
				std::cout << ", ";
			}
			std::cout << std::endl;
			std::cout << indent << "}" << std::endl;
		}
	};

	NothingSerializer nothingSerializer;

	template <typename EmitterInput,
			  typename EmitterOutput,
			  typename WorkerOutput,
			  typename CollectorOutput>
	FarmPattern<EmitterInput, EmitterOutput, WorkerOutput, CollectorOutput> Farm(
		SenderReceiver<EmitterInput> &_worldToEmitter,

		Wrapper<EmitterInput, EmitterOutput> &_emitter,
		SenderReceiver<EmitterOutput> &_emitterToWorkers,

		Wrapper<EmitterOutput, WorkerOutput> &_worker,
		SenderReceiver<WorkerOutput> &_workerToCollector,

		Wrapper<WorkerOutput, CollectorOutput> &_collector,
		SenderReceiver<CollectorOutput> &_collectorToWorld)
	{
		return FarmPattern<EmitterInput, EmitterOutput, WorkerOutput, CollectorOutput>(
			_worldToEmitter,
			_emitter, _emitterToWorkers,
			_worker, _workerToCollector,
			_collector, _collectorToWorld);
	};

	template <typename EmitterInput,
			  typename EmitterOutput,
			  typename WorkerOutput>
	FarmPattern<EmitterInput, EmitterOutput, WorkerOutput, Nothing> Farm(
		SenderReceiver<EmitterInput> &_worldToEmitter,

		Wrapper<EmitterInput, EmitterOutput> &_emitter,
		SenderReceiver<EmitterOutput> &_emitterToWorkers,

		Wrapper<EmitterOutput, WorkerOutput> &_worker,
		SenderReceiver<WorkerOutput> &_workerToCollector,

		Wrapper<WorkerOutput, Nothing> &_collector)
	{
		return FarmPattern<EmitterInput, EmitterOutput, WorkerOutput, Nothing>(
			_worldToEmitter,
			_emitter, _emitterToWorkers,
			_worker, _workerToCollector,
			_collector, nothingSerializer);
	};

	template <typename EmitterOutput,
			  typename WorkerOutput,
			  typename CollectorOutput>
	FarmPattern<Nothing, EmitterOutput, WorkerOutput, CollectorOutput> Farm(
		Wrapper<Nothing, EmitterOutput> &_emitter,
		SenderReceiver<EmitterOutput> &_emitterToWorkers,

		Wrapper<EmitterOutput, WorkerOutput> &_worker,
		SenderReceiver<WorkerOutput> &_workerToCollector,

		Wrapper<WorkerOutput, CollectorOutput> &_collector,
		SenderReceiver<CollectorOutput> &_collectorToWorld)
	{
		return FarmPattern<Nothing, EmitterOutput, WorkerOutput, CollectorOutput>(
			nothingSerializer,
			_emitter, _emitterToWorkers,
			_worker, _workerToCollector,
			_collector, _collectorToWorld);
	};

	template <typename EmitterOutput,
			  typename WorkerOutput>
	FarmPattern<Nothing, EmitterOutput, WorkerOutput, Nothing> Farm(
		Wrapper<Nothing, EmitterOutput> &_emitter,
		SenderReceiver<EmitterOutput> &_emitterToWorkers,

		Wrapper<EmitterOutput, WorkerOutput> &_worker,
		SenderReceiver<WorkerOutput> &_workerToCollector,

		Wrapper<WorkerOutput, Nothing> &_collector)
	{
		return FarmPattern<Nothing, EmitterOutput, WorkerOutput, Nothing>(
			nothingSerializer,
			_emitter, _emitterToWorkers,
			_worker, _workerToCollector,
			_collector, nothingSerializer);
	};
	
	template<typename T>
	class DummyCollector: public OutWrapper<T> {
		public:
		void Process(T &data) override {	}
	};

	template<typename T>
	class DummyStage: public Wrapper<T, T> {
		public:
		void Process(T &data) override {
			this->Emit(data);
		}
	};

} // namespace dspar