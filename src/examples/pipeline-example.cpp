#include "dspar/farm/farm.h"
#include "dspar/Pipeline.h"

// Source operator
class Source : public dspar::Emitter<int>
{
public:
    void Produce()
    {
        for (int i = 0; i <= 100; i++)
        {
            Emit(i);
        }
    };
};

// Middle operator
class Middle : public dspar::Worker<int, std::string>
{
public:
    void Process(int &i)
    {
        std::string output = "Hello World " + std::to_string(i);
        Emit(output);
    };
};

// ByPass 1 operator
class ByPass1 : public dspar::Wrapper<std::string, std::string>
{
public:
    void Process(std::string &data)
    {
        std::string output = data + " (Farm 1)";
        Emit(output);
    };
};

// ByPass 2 operator
class ByPass2 : public dspar::Wrapper<std::string, std::string>
{
public:
    void Process(std::string &data)
    {
        std::string output = data + " (Farm 2)";
        Emit(output);
    };
};

// Sink operator
class Sink : public dspar::Collector<std::string>
{
public:
    void Process(std::string &data)
    {
        printf("%s\n", data.c_str());
    };
};

int main(int argc, char **argv)
{
    // Serializers
    dspar::TrivialSendReceive<int> intSerializer;
    dspar::TrivialSendReceive<std::string> stringSerializer;

    // Stages
    Source source;
    Middle middle;
    ByPass1 byPass1;
    ByPass2 byPass2;
    Sink sink;

    // Farm 1
    auto farm1 = dspar::Farm(
        source, intSerializer,
        middle, stringSerializer,
        byPass1, stringSerializer);

    farm1.SetOnDemandScheduling(true);
    farm1.SetCollectorIsOrdered(false);
    farm1.SetWorkerReplicas(2);

    // Farm 2
    auto farm2 = dspar::Farm(
        stringSerializer,
        byPass2, stringSerializer,
        byPass2, stringSerializer,
        sink);

    farm2.SetOnDemandScheduling(true);
    farm2.SetCollectorIsOrdered(false);
    farm2.SetWorkerReplicas(2);

    // Pipeline of farms
    dspar::Pipeline pipeline;
    pipeline.Add(&farm1);
    pipeline.Add(&farm2);

    // Set the number of MPI processes dynamically
    dspar::MPIUtils mpiUtils;
    MPI_Comm comm = mpiUtils.SetTotalNumberOfProcesses(argc, argv, pipeline.GetTotalNumberOfProcessesNeeded() - 1);

    // Start the application
    pipeline.Start(comm);

    // Finalize MPI
    MPI_Finalize();
}