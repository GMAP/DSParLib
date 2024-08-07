#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <functional>
#include <algorithm>
#include "dspar.h"
#include <map>
#include "wrappers.h"
#include "SenderReceiver.h"
#include "Pipeline.h"

namespace dspar
{
	template <typename T>
	struct MessageToReorder
	{
		MessageToReorder(MessageHeader _header, T _data) : header(_header), data(_data){};
		MessageHeader header;
		T data;

		bool operator<(const MessageToReorder &m) const
		{
			return m.header.id < header.id;
		}
	};

	template <typename StageInput, typename StageOutput>
	class DSparNode final : public DSparLifecycle 
	{
	private:
		Wrapper<StageInput, StageOutput> &stage;
		SenderReceiver<StageInput> &inputReceiver;
		SenderReceiver<StageOutput> &outputSender;
		int numberOfTargetsWaitingToStop = -1;
		int numberOfSourcesWaitingToStop = -1;
		bool firstMessageAlreadyProcessed = false;
		DSParNodeConfiguration nodeConfiguration;
		dspar::CircularVector<int> nextStageRanks;
		dspar::CircularVector<int> sources;
		std::priority_queue<MessageToReorder<StageInput>> orderedMessages;
		uint64_t currentMessage = 0;

		MessageHeader latestMessageHeader;
		bool emitShouldUseLatestHeader = false;
		AfterStart afterStart;

		int processCalls = 0;
		//int emitCalls = 0;
		//int onReceiveBeforeRecv = 0;
		//int onReceiveAfterRecv = 0;

	private:
		void WaitDemandAndEmit(StageOutput &data, MessageHeader &previousHeader)
		{
			TRACE();
			TRLABEL("FarmStage: Waiting for demand...");
			DemandSignal demand = this->WaitForDemand();
			TRLABEL("FarmStage: Emitting");

			int targetRank = demand.sender;

#ifdef DSPARTIMINGS
			Duration d(previousHeader.ts);
			TimePoint tp(d);
			Duration thisMsgComputeTime = this->currentMessageEndComputing - this->currentMessageStartComputing;
			auto totalComputeTime = Duration(previousHeader.totalComputeTime) + thisMsgComputeTime;

			/*Duration thisMsgIoTime = this->currentMessageEndRecv - tp;
		Duration thisMsgRecvTime = this->currentMessageEndRecv - this->currentMessageStartRecv;

		auto totalIoTime = Duration(previousHeader.totalIoTime) + thisMsgIoTime;
		auto totalReceiveTime = Duration(previousHeader.totalReceiveTime) + thisMsgRecvTime;
	*/
			auto timings = Timings{
				//.totalReceiveTime = totalReceiveTime.count(),
				//.totalIoTime = totalIoTime.count(),
				//.totalProcessingTime = totalProcessingTime.count(),
				.totalComputeTime = totalComputeTime.count(),
			};

			MessageHeader header = this->GetSender().StartSendingMessageTo(targetRank, previousHeader.id, previousHeader.ts, timings);
#else
			MessageHeader header = this->GetSender().StartSendingMessageTo(targetRank, previousHeader.id);
#endif

			TRLABEL("FarmStage outputSender.Send(this->GetSender(), header, data);");
			outputSender.Send(this->GetSender(), header, data);
		};

		void WaitDemandAndEmit(StageOutput &data)
		{
			TRACE();
			TRLABEL("FarmStage: Waiting for demand...");
			DemandSignal demand = this->WaitForDemand();
			TRLABEL("FarmStage: Emitting");

			int targetRank = demand.sender;
#ifdef DSPARTIMINGS
			Duration thisMsgComputeTime = this->currentMessageEndComputing - this->currentMessageStartComputing;
			auto timings = Timings{
				//.totalReceiveTime = 0,
				//.totalIoTime = 0,
				.totalComputeTime = thisMsgComputeTime.count(),
			};
			MessageHeader header = this->GetSender().StartSendingMessageTo(targetRank, std::numeric_limits<uint64_t>::max(), 0, timings);
#else
			MessageHeader header = this->GetSender().StartSendingMessageTo(targetRank, std::numeric_limits<uint64_t>::max());
#endif
			TRLABEL("FarmStage outputSender.Send(this->GetSender(), header, data);");
			outputSender.Send(this->GetSender(), header, data);
		};

		void EmitRoundRobin(StageOutput &data, MessageHeader &previousHeader)
		{
			TRACE();
			TRLABEL("FarmStage: Emitting");

#ifdef DSPARTIMINGS
			Duration d(previousHeader.ts);
			TimePoint tp(d);
			Duration thisMsgComputeTime = this->currentMessageEndComputing - this->currentMessageStartComputing;
			auto totalComputeTime = Duration(previousHeader.totalComputeTime) + thisMsgComputeTime;

			/*
		Duration thisMsgIoTime = this->currentMessageEndRecv - tp;
		Duration thisMsgRecvTime = this->currentMessageEndRecv - this->currentMessageStartRecv;
		Duration thisMsgProcessingTime = this->currentMessageEndProcessing - this->currentMessageStartProcessing;

		auto totalIoTime = Duration(previousHeader.totalIoTime) + thisMsgIoTime;
		auto totalReceiveTime = Duration(previousHeader.totalReceiveTime) + thisMsgRecvTime;
		auto totalProcessingTime = Duration(previousHeader.totalProcessingTime) + thisMsgProcessingTime;
*/
			auto timings = Timings{
				//.totalReceiveTime = totalReceiveTime.count(),
				//.totalIoTime = totalIoTime.count(),
				.totalComputeTime = totalComputeTime.count(),
			};

			MessageHeader header = this->GetSender().StartSendingMessageTo(nextStageRanks.Next(), previousHeader.id, previousHeader.ts, timings);
#else
			MessageHeader header = this->GetSender().StartSendingMessageTo(nextStageRanks.Next(), previousHeader.id);
#endif
			TRLABEL("FarmStage outputSender.Send(this->GetSender(), header, data);");
			outputSender.Send(this->GetSender(), header, data);
		};

		void EmitRoundRobin(StageOutput &data)
		{
			TRACE();
			TRLABEL("FarmStage: Emitting");
#ifdef DSPARTIMINGS
			Duration thisMsgComputeTime = this->currentMessageEndComputing - this->currentMessageStartComputing;

			auto timings = Timings{
				//.totalReceiveTime = 0,
				//.totalIoTime = 0,
				.totalComputeTime = thisMsgComputeTime.count(),
			};
			MessageHeader header = this->GetSender().StartSendingMessageTo(nextStageRanks.Next(),
																		   std::numeric_limits<uint64_t>::max(), 0, timings);
#else
			MessageHeader header = this->GetSender().StartSendingMessageTo(nextStageRanks.Next(), std::numeric_limits<uint64_t>::max());

#endif
			TRLABEL("FarmStage outputSender.Send(this->GetSender(), header, data);");
			outputSender.Send(this->GetSender(), header, data);
		};

		void Emit(StageOutput &data, MessageHeader &previousHeader)
		{
			if (nodeConfiguration.WaitForDemandDownstream)
			{
				WaitDemandAndEmit(data, previousHeader);
			}
			else
			{
				EmitRoundRobin(data, previousHeader);
			}
		};

		void Emit(StageOutput &data)
		{
			if (nodeConfiguration.WaitForDemandDownstream)
			{
				WaitDemandAndEmit(data);
			}
			else
			{
				EmitRoundRobin(data);
			}
		};

		void WaitFinalDemands()
		{
			TRACE();
			TRLABEL("FarmStage Wait Final Demands");
			while (true)
			{
				DemandSignal demand = this->WaitForDemand();
				DSPAR_DEBUG("Got final demand from " << demand.sender << ", numberOfSourcesWaitingToStop = " << nextStageRanks.Count());

				GetSender().SendStopMessageTo(demand.sender);

				//DSPAR_DEBUG("Sent final demand stop signal to " << demand.sender);

				nextStageRanks.Remove(demand.sender);

				if (nextStageRanks.IsEmpty())
				{
					DSPAR_DEBUG("No more demands to wait.");
					return;
				}
				DSPAR_DEBUG("Awaiting more final demands");
			}
		};

	public:
		DSparNode(
			Wrapper<StageInput, StageOutput> &_stage,
			SenderReceiver<StageInput> &_inputReceiver,
			SenderReceiver<StageOutput> &_outputSender,
			std::vector<int> _targets,
			std::vector<int> _sources,
			DSParNodeConfiguration _nodeConfiguration) : stage(_stage), inputReceiver(_inputReceiver), outputSender(_outputSender),
													  nodeConfiguration(_nodeConfiguration),
													  nextStageRanks(_targets), sources(_sources)

		{
			
			std::function<void(StageOutput)> func = [this](StageOutput data) {
#ifdef DSPARTIMINGS
				if (this->sources.Count() > 0)
				{
					this->currentMessageEndComputing = Clock::now();
				}
#endif

				if (this->emitShouldUseLatestHeader)
				{
					//this->emitCalls++;
					this->Emit(data, this->latestMessageHeader);
				}
				else
				{
					//this->emitCalls++;
					this->Emit(data);
				}
			};
			stage.SetEmitter(func);
		}

		AfterStart OnStart() override
		{
			if (this->nextStageRanks.Count() == 0)
			{
				dspar::globals::isCollector = true;
				dspar::globals::collectorRank = this->GetMyRank();
			}
			else if (this->sources.Count() == 0) {
				dspar::globals::isEmitter = true;
				dspar::globals::emitterRank = this->GetMyRank();
			}

			TRACE();
			TRLABEL("FarmStage Start");

			stage.Start();

			TRLABEL("FarmStage Produce");
			stage.Produce();

			if (nodeConfiguration.AskForDemandUpstream)
			{
				TRBLOCK("Asking demand...");
				this->SendDemand(sources.Next(), 1);
			}

			if (sources.Count() > 0)
			{
				return AfterStart::ReceiveMessages;
			}
			else
			{
				return AfterStart::StopNode;
			}
		};

		virtual void OnReceiveMessage(MessageHeader &msg) override
		{
			TRACE();
			TRLABEL("OnReceiveMessage");

			StageInput data = inputReceiver.Receive(this->GetReceiver(), msg);
#ifdef DSPARTIMINGS
			this->currentMessageEndRecv = Clock::now();
#endif
			if (nodeConfiguration.Ordered)
			{
				ReorderAndProcess(msg, data);
			}
			else
			{
				ProcessInput(data, msg);
			}
		};

#ifdef DSPARTIMINGS
		void PrintTiming(MessageHeader &header, Timings &timings)
		{
			std::cout.precision(10);
			//std::cout << "ts = "<< header.ts << " endRecv = "<< this->currentMessageEndRecv.time_since_epoch().count() <<std::endl;
			std::cout << header.id << ": ts = \t" << header.ts << "\ttime = \t" << std::fixed;
			std::cout << std::chrono::duration<double>(Duration(timings.totalTime)).count();
			std::cout << "\tI/O = \t";
			std::cout << std::chrono::duration<double>(Duration(timings.totalIoTime)).count();
			std::cout << "\tCPU = \t";
			std::cout << std::chrono::duration<double>(Duration(timings.totalComputeTime)).count();
			std::cout << "\n";
		}
#endif

		void ProcessInput(StageInput &input, MessageHeader &header)
		{
			TRACE();
			TRLABEL("ProcessInput: Before processing input");

			latestMessageHeader = header;

			if (!firstMessageAlreadyProcessed)
			{
				TRBLOCK("OnDemand worker: On First Frame");
				firstMessageAlreadyProcessed = true;
				stage.OnFirstItem(input);
			}

			emitShouldUseLatestHeader = true;

#ifdef DSPARTIMINGS
			this->currentMessageStartComputing = Clock::now();
#endif

			TRLABEL("ProcessInput: stage.Process()");

			stage.Process(input);

			emitShouldUseLatestHeader = false;

#ifdef DSPARTIMINGS
			
			this->currentMessageEndComputing = Clock::now();

			Duration d(header.ts);
			TimePoint tp(d);

			//Duration thisMsgIoTime = this->currentMessageEndRecv - tp;
			//Duration thisMsgRecvTime = this->currentMessageEndRecv - this->currentMessageStartRecv;
			Duration totalProcessingTime = this->currentMessageEndComputing - tp;
			Duration collectorComputeTime = this->currentMessageEndComputing - this->currentMessageStartComputing;

			//auto totalIoTime = Duration(header.totalIoTime) + thisMsgIoTime;
			//auto totalReceiveTime = Duration(header.totalReceiveTime) + thisMsgRecvTime;
			auto totalComputeTime = Duration(header.totalComputeTime) + collectorComputeTime;

			Timings timings;
			timings.totalIoTime = (totalProcessingTime - totalComputeTime).count();
			timings.totalTime = totalProcessingTime.count();
			timings.totalComputeTime = totalComputeTime.count();

			
			if (this->nextStageRanks.Count() == 0)
			{
				PrintTiming(header, timings);
				dspar::globals::collectorTimings.push_back(timings);
			} else {
				if (header.id % 1 == 0) {
				//	PrintTiming(header, timings);
				}
			}
#endif

			if (nodeConfiguration.AskForDemandUpstream)
			{
				TRLABEL("ProcessInput: Asking for demand");
				this->SendDemand(sources.Next(), 1);
			}
		}

		void ReorderAndProcess(MessageHeader &msg, StageInput &data)
		{
			TRACE();
			//std::cout << "Unordered: "<<orderedMessages.size()<<std::endl;
			if (msg.id <= this->currentMessage)
			{
				{
					TRBLOCK("Collector unordered: Processing data");
					ProcessInput(data, msg);
				}

				this->currentMessage++;

				while (true)
				{
					//	std::cout << "Ordered messages size " << orderedMessages.size() << std::endl;

					if (orderedMessages.size() == 0)
					{
						return;
					}

					auto top = orderedMessages.top();

					if (top.header.id > this->currentMessage)
					{
						//		std::cout << "Breaking, top ID " << top.header.id << " current " << this->currentMessage << std::endl;
						break;
					}

					{
						//		std::cout << "Processing unordered ID " << top.header.id << std::endl;
						TRBLOCK("Collector unordered: Processing data");
						ProcessInput(top.data, top.header);
					}

					orderedMessages.pop();
					//std::cout << "Popped" << std::endl;
					this->currentMessage++;
					processCalls++;
				}
			}
			else
			{
				MessageToReorder<StageInput> msgReorder(msg, data);
				orderedMessages.push(msgReorder);
			}
		}

		void OnStop() override
		{
			TRACE();
			DSPAR_DEBUG("FarmStage running OnStop");
			stage.End();

			//We wait on final demands here because we can only tell the workers to stop when our sources stop.
			if (nodeConfiguration.WaitForDemandDownstream)
			{
				DSPAR_DEBUG("FarmStage OnStop running WaitFinalDemands");
				WaitFinalDemands();
			}
			else
			{
				DSPAR_DEBUG("FarmStage sending stop to targets");
				for (int rank : this->nextStageRanks.Data())
				{
					DSPAR_DEBUG("FarmStage sending stop to " << rank);
					this->GetSender().SendStopMessageTo(rank);
				}
			}
		};

		virtual StopResponse OnReceiveStop(MessageHeader &msg) override
		{
			TRACE();
			this->sources.Remove(msg.sender);

			if (this->sources.IsEmpty())
			{
				return StopResponse::Stop;
			}
			else
			{
				if (nodeConfiguration.AskForDemandUpstream)
				{
					TRLABEL("OnDemand worker: Asking for demand during stop");
					this->SendDemand(this->sources.Next(), 1);
				}

				return StopResponse::Ignore;
			}
		};
	};

} // namespace dspar