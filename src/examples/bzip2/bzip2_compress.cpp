#include "dspar/farm/farm.h"
#include <bzip2/bzlib.h>
#include <fstream>

// Source operator
class Source : public dspar::Emitter<std::string>
{
private:
    std::ifstream inputFile;

public:
    void Start()
    {
        // Open the input file
        std::string filePath = "workloads/bzip2/input.txt";
        inputFile = std::ifstream(filePath, std::ios::binary);
        if (!inputFile.good())
            printf("File %s not found.\n", filePath.c_str());
    }

    void Stop()
    {
        // Close the input file
        inputFile.close();
    }

    void Produce()
    {
        // Get the input file size
        inputFile.seekg(0, std::ios::end);
        const size_t fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);

        // Initialization
        const size_t blockSize = 900000;
        size_t bytesLeft = fileSize;

        // Send the file blocks
        while (bytesLeft > 0)
        {
            size_t sliceSize = std::min(blockSize, bytesLeft);
            bytesLeft -= sliceSize;

            // Read data
            std::string bufferSlice;
            bufferSlice.resize(sliceSize);
            inputFile.read(bufferSlice.data(), sliceSize);

            // Send data
            Emit(bufferSlice);
        }
    };
};

// Middle operator
class Middle : public dspar::Worker<std::string, std::string>
{
public:
    void Process(std::string &input)
    {
        // Compress the slice
        bz_stream bzBuffer = {0};
        BZ2_bzCompressInit(&bzBuffer, 9, 0, 30);

        int outputBufferSize = input.size() * 1.01 + 600;
        std::string outputBuffer;
        outputBuffer.resize(outputBufferSize);

        char* data = (char*)input.data();
        size_t size = input.size();

        bzBuffer.next_in = data;
        bzBuffer.avail_in = size;
        bzBuffer.next_out = outputBuffer.data();
        bzBuffer.avail_out = outputBuffer.size();

        BZ2_bzCompress(&bzBuffer, BZ_FINISH);
        BZ2_bzCompressEnd(&bzBuffer);

        outputBuffer.resize(bzBuffer.total_out_lo32);

        Emit(outputBuffer);
    };
};

// Sink operator
class Sink : public dspar::Collector<std::string>
{
private:
    std::ofstream outputFile;

public:
    void Start()
    {
        // Create the output file
        std::string outputFilename = "workloads/bzip2/output.txt.bz2";
        outputFile = std::ofstream(outputFilename, std::ios::binary | std::ios::trunc);
        if (!outputFile.good())
            throw std::runtime_error("Could not create output file " + outputFilename);
    }

    void End()
    {
        // Flush and close the files
        outputFile.flush();
        outputFile.close();
    }

    void Process(std::string &input)
    {
        // Write the compressed data to the output file
        outputFile.write(input.data(), input.size());
    };
};

int main(int argc, char **argv)
{
    // Serializers
    dspar::TrivialSendReceive<std::string> stringSerializer;

    // Stages
    Source source;
    Middle middle;
    Sink sink;

    // Farm pattern
    auto farm = dspar::Farm(
        source, stringSerializer,
        middle, stringSerializer,
        sink);

    farm.SetCollectorIsOrdered(true);
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