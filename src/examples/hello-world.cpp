#include "dspar/farm/farm.h"

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
    Sink sink;

    // Farm pattern
    auto farm = dspar::Farm(
        source, intSerializer,
        middle, stringSerializer,
        sink
    );

    farm.SetCollectorIsOrdered(false);
    farm.SetOnDemandScheduling(true);
    farm.SetWorkerReplicas(2);

    // Set the number of MPI processes dynamically
    dspar::MPIUtils mpiUtils;
    MPI_Comm comm = mpiUtils.SetTotalNumberOfProcesses(argc, argv, farm.GetTotalNumberOfProcessesNeeded() - 1);

    // Start the farm
    farm.Start(comm, 0);

    // Finalize MPI
    MPI_Finalize();
}