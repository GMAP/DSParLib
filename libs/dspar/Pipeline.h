#pragma once

#include "mpi.h"
#include <vector>
#include "dspar.h"

namespace dspar
{
    class AbstractPipelineElement
    {
    public:
        virtual int Start(MPI_Comm comm, int startingRank,
                          std::vector<int> inputRanks,
                          std::vector<int> outputRanks) = 0;

        virtual std::vector<int> GetInputOffsetRanks() = 0;
        virtual std::vector<int> GetOutputOffsetRanks() = 0;
        virtual int GetTotalNumberOfProcessesNeeded() = 0;
        virtual void PrintGraph(int startingRank, int indentationLevel) = 0;
    };

    struct Plan
    {
        AbstractPipelineElement *node;
        int startingRank;
        std::vector<int> sources;
        std::vector<int> targets;
    };

    class Pipeline
    {
    private:
        std::vector<AbstractPipelineElement *> nodes;

        std::vector<int> Increment(std::vector<int> vec, int amount)
        {
            std::vector<int> res;
            for (int i : vec)
            {
                res.push_back(i + amount);
            }
            return res;
        }

    public:

        Pipeline() { }

        Pipeline(AbstractPipelineElement* p) { Add(p); }

        template<typename ... Args> 
        Pipeline(AbstractPipelineElement* p, Args... args) { Add(p); Pipeline(args...); }

        Pipeline(std::initializer_list<AbstractPipelineElement*> list) {
            for (auto p: list) {
                Add(p);
            }
        }

        void Add(AbstractPipelineElement *node)
        {
            nodes.push_back(node);
        }

        void Start(MPI_Comm comm)
        {
            auto plans = GetPlans();

            dspar::MPIUtils utils;
            int myRank = utils.GetMyRank(comm);

            for (auto p : plans)
            {
                if (myRank >= p->startingRank && myRank <= (p->startingRank + p->node->GetTotalNumberOfProcessesNeeded() - 1))
                {
                    LOG_DEBUG("Process " << myRank << " started, node starting rank is " << p->startingRank);
                    p->node->Start(comm, p->startingRank, p->sources, p->targets);
                    LOG_DEBUG("Process " << myRank << " finished");
                    break;
                }
                delete p;
            }
            MPI_Barrier(comm);
        }

        std::vector<Plan *> GetPlans()
        {
            std::vector<Plan *> plans;

            //initialize plans
            for (int i = 0; i < nodes.size(); i++)
            {
                plans.push_back(new Plan{
                    .node = nodes[i],
                    .startingRank = -1,
                    .sources = std::vector<int>(),
                    .targets = std::vector<int>()});
            }

            if (plans.size() > 1)
            {
                int processesAssigned = 0;
                for (int i = 0; i < plans.size(); i++)
                {
                    Plan *p = plans[i];
                    if (i == 0)
                    { //first node
                        p->sources = {};
                        p->targets = Increment(plans[i + 1]->node->GetInputOffsetRanks(), p->node->GetTotalNumberOfProcessesNeeded());
                        p->startingRank = 0;
                        processesAssigned += p->node->GetTotalNumberOfProcessesNeeded();
                    }
                    else if (i == plans.size() - 1)
                    { //last node
                        p->sources = Increment(plans[i - 1]->node->GetOutputOffsetRanks(), plans[i - 1]->startingRank);
                        p->targets = {};
                        p->startingRank = processesAssigned;
                        processesAssigned += p->node->GetTotalNumberOfProcessesNeeded();
                    }
                    else
                    { //in-between nodes
                        p->sources = Increment(plans[i - 1]->node->GetOutputOffsetRanks(), plans[i - 1]->startingRank);
                        p->startingRank = processesAssigned;
                        processesAssigned += p->node->GetTotalNumberOfProcessesNeeded();
                        p->targets = Increment(plans[i + 1]->node->GetInputOffsetRanks(), processesAssigned);
                    }
                }
            }
            else if (plans.size() == 1)
            { //corner case
                Plan *p = plans[0];
                p->sources = {};
                p->startingRank = 0;
                p->targets = {};
            }
            return plans;
        }

        int GetTotalNumberOfProcessesNeeded()
        {
            int total = 0;
            for (int i = 0; i < nodes.size(); i++)
            {
                total += (*nodes[i]).GetTotalNumberOfProcessesNeeded();
            }
            return total;
        }

        void PrintPlans()
        {
            for (auto p : GetPlans())
            {
                p->node->PrintGraph(p->startingRank, 1);
                delete p;
            }
        }
    };

} // namespace dspar