#include "wrappers.h"
#include "dspar.h"
#include "Pipeline.h"
#include "DSparNode.h"

#ifdef TRACE_PROFILER

#define TRACE_RANKTHREAD(rank)                 \
    std::stringstream ss;                      \
    ss << "Rank" << rank;                      \
    const char *threadname = ss.str().c_str(); \
    TRTHREADPROC(threadname);

#else
#define TRACE_RANKTHREAD(rank)
#endif

namespace dspar
{
    template <typename TIn, typename TOut>
    class PipelineStage : public AbstractPipelineElement
    {
    private:
        Wrapper<TIn, TOut> &stage;
        SenderReceiver<TIn> &inputReceiver;
        SenderReceiver<TOut> &outputSender;
        bool ordered = false;

    public:
        PipelineStage(Wrapper<TIn, TOut> &_stage,
                        SenderReceiver<TIn> &_inputReceiver,
                        SenderReceiver<TOut> &_outputSender) : stage(_stage),
                                                               inputReceiver(_inputReceiver),
                                                               outputSender(_outputSender)
        {
        }

        void SetOrdered(bool _ordered) {
            this->ordered = ordered;
        }

        int Start(MPI_Comm comm, int startingRank,
                  std::vector<int> inputRanks,
                  std::vector<int> outputRanks) override
        {

            dspar::MPIUtils utils;
            int myRank = utils.GetMyRank(comm);

            TRACE_RANKTHREAD(myRank);
            TRACE();

            if (myRank < startingRank)
            {
                return GetTotalNumberOfProcessesNeeded() + startingRank;
            }

            DSParNodeConfiguration nodeConfig;
            nodeConfig.AskForDemandUpstream = false;
            nodeConfig.Ordered = ordered;
            nodeConfig.WaitForDemandDownstream = false;

            DSparNode<TIn, TOut> pipeStage(stage, inputReceiver, outputSender,
                                                   outputRanks, inputRanks, nodeConfig);

            pipeStage.StartNode(comm);

            return GetTotalNumberOfProcessesNeeded() + startingRank;
        };

        std::vector<int> GetInputOffsetRanks() override { return {0}; };
        std::vector<int> GetOutputOffsetRanks() override { return {0}; };
        int GetTotalNumberOfProcessesNeeded() override { return 1; };
        void PrintGraph(int startingRank, int indentationLevel) override
        {
            std::string indent = "";
            For(indentationLevel)
            {
                indent += "    ";
            }
            std::cout << indent << "PipelineStage: {" << std::endl;
            std::cout << indent << "    Process: " << startingRank << std::endl;
            std::cout << indent << "}" << std::endl;
        };
    };


	template <typename TIn, typename TOut>
	PipelineStage<TIn, TOut> Stage(
            Wrapper<TIn, TOut> &_stage,
            SenderReceiver<TIn> &_inputReceiver,
            SenderReceiver<TOut> &_outputSender) {
		return PipelineStage<TIn, TOut>(_stage, _inputReceiver, _outputSender);
	};

	template <typename TIn>
	PipelineStage<TIn, Nothing> Stage(
            Wrapper<TIn, Nothing> &_stage,
            SenderReceiver<TIn> &_inputReceiver) {
		return PipelineStage<TIn, Nothing>(_stage, _inputReceiver, nothingSerializer);
	};

	template <typename TOut>
	PipelineStage<Nothing, TOut> Stage(
            Wrapper<Nothing, TOut> &_stage,
            SenderReceiver<TOut> &_outputSender) {
		return PipelineStage<Nothing, TOut>(_stage, nothingSerializer, _outputSender);
	};

} // namespace dspar