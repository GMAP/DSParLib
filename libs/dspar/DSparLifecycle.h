#pragma once

#include "dspar.h"
#include <functional>
namespace dspar
{

    std::vector<std::function<void()>> afterProcessMessageHandlers;
    void DeferAfterProcessMessage(std::function<void()> functionToRun) {
        afterProcessMessageHandlers.push_back(functionToRun);
    }


    enum AfterStart
    {
        StopNode = 0,
        ReceiveMessages = 1
    };

    enum StopResponse
    {
        Stop = 0,
        Ignore = 1
    };

    class DSparLifecycle
    {
    private:
        MPI_Comm *comm;
        dspar::MPIUtils mpiUtils;
        MPISender *Sender;
        MPIReceiver *Receiver;

    protected:
#ifdef DSPARTIMINGS
        TimePoint currentMessageStartRecv = Clock::now();
        TimePoint currentMessageEndRecv = Clock::now();

        TimePoint currentMessageStartComputing = Clock::now();
        TimePoint currentMessageEndComputing = Clock::now();
#endif
    public:
        DSparLifecycle()
        {
            this->Sender = NULL;
            this->Receiver = NULL;
        };

        void StartNode(MPI_Comm _comm)
        {
            LOG_DEBUG("StartNode called");
            TRACE();
            MPISender sender(_comm);
            MPIReceiver receiver(_comm);

            this->comm = &_comm;
            this->Sender = &sender;
            this->Receiver = &receiver;

            AfterStart behavior = OnStart();

            int rank = mpiUtils.GetMyRank(_comm);
            LOG_DEBUG("StartNode running on rank "<<rank);
            if (behavior == AfterStart::ReceiveMessages)
            {
                while (true)
                {
                    TRBLOCK("Message loop iteration");
#ifdef DSPARTIMINGS
                    this->currentMessageStartRecv = Clock::now();
#endif
                    MessageHeader msg;
                    {
                        TRBLOCK("Waiting for message header...");
                        msg = receiver.StartReceivingMessage();
                    }

                    if (msg.type == STOP_TYPE)
                    {
                        TRBLOCK("Got stop signal");
                        StopResponse response = OnReceiveStop(msg);
                        if (response == StopResponse::Stop)
                        {
                            break;
                        }
                    }
                    else if (msg.type == MESSAGE_TYPE)
                    {
                        TRBLOCK("Calling OnReceiveMessage");
                        OnReceiveMessage(msg);

                        for (int i = afterProcessMessageHandlers.size() - 1; i >= 0; i--) {
                            afterProcessMessageHandlers[i]();
                        }
                        afterProcessMessageHandlers.clear();
                    }
                }
            }
            DSPAR_DEBUG("Stopping node " << rank);

            OnStop();
            DSPAR_DEBUG("STOPPED node " << rank);
        }

        DemandSignal WaitForDemand()
        {
            return GetReceiver().StartReceivingDemand();
        }

        DemandSignal SendDemand(int target, int demand)
        {
            return GetSender().SendDemandSignalTo(target, demand);
        }

        AsyncMPIRequest<DemandSignal> SendDemandAsync(int target, int demand)
        {
            return GetSender().SendDemandSignalToAsync(target, demand);
        }

        template <typename T>
        AsyncMPIRequest<T> Await(AsyncMPIRequest<T> task)
        {
            return GetSender().Await(task);
        }

        virtual AfterStart OnStart()
        {
            return AfterStart::ReceiveMessages;
        }
        virtual void OnStop() = 0;
        virtual StopResponse OnReceiveStop(MessageHeader &msg) = 0;
        virtual void OnReceiveMessage(MessageHeader &msg) = 0;

        int GetMyRank()
        {
            return this->mpiUtils.GetMyRank(*comm);
        }

        MPISender &GetSender()
        {
            return *this->Sender;
        }

        MPIReceiver &GetReceiver()
        {
            return *this->Receiver;
        }
    };
} // namespace dspar
