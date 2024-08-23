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
        std::string filePath = "workloads/bzip2/output.txt.bz2";
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

        // Bzip2 header or "magic number"
        // See more at: https://en.wikipedia.org/wiki/Bzip2#File_format
        const std::string magicNumber = "BZh91AY&SY";

        // Block size of 900KB
        const size_t blockSize = 900000;

        // List of bzip2 blocks
        std::vector<size_t> blocks;

        // Bytes left to read
        size_t bytesLeft = fileSize;

        // Read positions
        size_t posInit = 0;
        size_t posEnd = 0;

        // Find bzip2 blocks
        while (bytesLeft > 0)
        {
            posInit = posEnd;

            // Calculate the slice size
            size_t sliceSize = std::min(blockSize, bytesLeft);

            // If the slice size is equal to the magic number size
            // it means that we reach the end of the file
            if (sliceSize == magicNumber.size())
            {
                blocks.push_back(fileSize);
                break;
            }

            // Set the read position
            inputFile.seekg(posInit);

            // Read data
            std::string bufferSlice;
            bufferSlice.resize(sliceSize);
            inputFile.read(bufferSlice.data(), sliceSize);

            // Find the magic number (bzip2 block header)
            size_t magicPos = bufferSlice.find(magicNumber);
            if (magicPos != std::string::npos)
            {
                // If the magic number is found, add the block to the list
                // and set the end position at the end of the block
                blocks.push_back(posInit + magicPos);
                posEnd = posInit + magicPos + magicNumber.size();
            }
            else
            {
                // Otherwise, set the end position to the end of the slice less the magic number size
                // in case the magic number is split between two slices
                posEnd = posInit + sliceSize - magicNumber.size();
            }

            // Subtract the read bytes from the file size
            bytesLeft -= posEnd - posInit;
        }

        // Send the blocks
        for (int i = 0; i < blocks.size() - 1; i++)
        {
            posInit = blocks[i];
            posEnd = blocks[i + 1];

            // Set the read position
            inputFile.seekg(posInit);

            // Read data
            std::string bufferSlice;
            bufferSlice.resize(posEnd - posInit);
            inputFile.read(bufferSlice.data(), posEnd - posInit);

            Emit(bufferSlice);
        }
    }
};

// Middle operator
class Middle : public dspar::Worker<std::string, std::string>
{
private:
    // Block size of 900KB
    const size_t blockSize = 900000;

public:
    void Process(std::string &input)
    {
        // Decompress the slice
        bz_stream bzBuffer = {0};
        BZ2_bzDecompressInit(&bzBuffer, 0, 0);

        std::string outputBuffer;
        outputBuffer.resize(blockSize);

        char* data = (char*)input.data();
        size_t size = input.size();

        bzBuffer.next_in = data;
        bzBuffer.avail_in = size;
        bzBuffer.next_out = outputBuffer.data();
        bzBuffer.avail_out = outputBuffer.size();

        BZ2_bzDecompress(&bzBuffer);
        BZ2_bzDecompressEnd(&bzBuffer);

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
        std::string outputFilename = "workloads/bzip2/output.txt";
        outputFile = std::ofstream(outputFilename, std::ios::binary | std::ios::trunc);
        if (!outputFile.good())
        {
            printf("Could not create output file %s.\n", outputFilename.c_str());
            return;
        }
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