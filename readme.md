# DSParLib - Distributed Stream Parallelism

This is a library for research on distributed stream parallelism in HPC.
<br>It uses MPI for communication between nodes in the network.

Features:
 - Pipeline and Farm pattern
 - Dynamic MPI process spawning
 - Stream stopping
 - Standalone stages
 - Pipeline composition with farms and stages
 - Abstractions for data serializing, allowing low-level MPI serialization (including definition of data types) and a higher-level send/receive API (MPI-like, but with C++ metaprogramming to make it easier)

# How to cite this work
Löff, J.; Hoffmann, R. B.; Pieper, R.; Griebler, D.; Fernandes, L. G. **“DSParLib: A C++ Template Library for Distributed Stream Parallelism”**, *International Journal of Parallel Programming*, vol. 50–5, 2022, pp. 454–485. [[PDF]](https://doi.org/10.1007/s10766-022-00737-2)

## BibTeX
```
@article{LOFF:IJPP:22,
	title={{DSParLib: A C++ Template Library for Distributed Stream Parallelism}},
	author={Júnior Löff and Renato Barreto Hoffmann and Ricardo Pieper and Dalvan Griebler and Luiz Gustavo Fernandes},
	journal={International Journal of Parallel Programming},
	volume={50},
	number={5},
	pages={454-485},
	year={2022},
	manth={October},
	publisher={Springer},
	doi={10.1007/s10766-022-00737-2},
	url={https://doi.org/10.1007/s10766-022-00737-2}
}
```

# Dependencies

### OpenMPI
```
sudo apt install openmpi-bin openmpi-common openmpi-doc libopenmpi-dev
```

# Compilation and usage
The DSParLib programs are compiled using the standard `mpic++` wrapper.
<br>Example of compilation command:
```
mpic++ -std=c++11 -I libs/ src/examples/hello-world.cpp -o hello-world.out
```

## Execution
Independent of the number of replicated stages, the DSParLib program always must be executed using the `-np 1` flag, the library automatically creates the required number of processes at runtime.
```
mpirun -np 1 hello-world.out
```

## Simple application example
This example consists of a very simple application, which aims to demonstrate the basic concepts of the DSParLib usage.

In this application, the source operator produce numbers from 0 to 99, the middle operator concatenates the string "Hello World" to the received number, and the sink operator just prints the received message over the output console.

More examples can be found at `src/examples` folder.

```C++
#include "dspar/farm/farm.h"

// Source operator
class Source : public dspar::Emitter<int>
{
public:
    void Produce()
    {
        for (int i = 0; i < 100; i++)
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
```