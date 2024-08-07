#include "dspar/farm/farm.h"

// Source operator
class Source : public dspar::Emitter<int>
{
public:
    void Produce()
    {
        for (int i = 2; i <= 10000; i++)
        {
            Emit(i);
        }
    };
};

// Middle operator
class Middle : public dspar::Worker<int, bool>
{
public:
    void Process(int &i)
    {
        bool isPrime = true;
        for (int j = 2; j < i; j++)
        {
            if (i % j == 0)
            {
                isPrime = false;
                break;
            }
        }
        Emit(isPrime);
    };
};

// Sink operator
class Sink : public dspar::Collector<bool>
{
public:
    int primes = 0;
    void Process(bool &data)
    {
        if (data)
        {
            primes++;
        }
    };

    void End() override
    {
        std::cout << "Primes: " << primes << std::endl;
    };
};

int main(int argc, char **argv)
{
    // Serializers
    dspar::TrivialSendReceive<int> intSerializer;
    dspar::TrivialSendReceive<bool> boolSerializer;

    // Operators
    Source source;
    Middle middle;
    Sink sink;

    // Farm
    auto farm = dspar::Farm(
        source, intSerializer,
        middle, boolSerializer,
        sink
    );

    farm.SetCollectorIsOrdered(false);
    farm.SetOnDemandScheduling(true);
    farm.SetWorkerReplicas(2);

    // Initialize the MPI environment and create the required processes dynamically
    dspar::MPIUtils mpiUtils;
    MPI_Comm comm = mpiUtils.SetTotalNumberOfProcesses(argc, argv, farm.GetTotalNumberOfProcessesNeeded() - 1);

    // Start the farm
    farm.Start(comm, 0);

    // Finalize the MPI environment
    MPI_Finalize();
}