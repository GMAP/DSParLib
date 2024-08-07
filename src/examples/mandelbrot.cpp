#include "dspar/farm/farm.h"

// Structure to store the line to render
struct LineToRender
{
    char *M;
    int size;
    int line;
};

// Source operator
class GenerateWork : public dspar::Wrapper<dspar::Nothing, LineToRender>
{
private:
    int numberOfLines;

public:
    GenerateWork(int numberOfLines)
    {
        this->numberOfLines = numberOfLines;
    };
    void Produce() override
    {
        for (int i = 0; i < numberOfLines; i++)
        {
            LineToRender lineToRender;
            lineToRender.M = NULL;
            lineToRender.line = i;
            lineToRender.size = 0;
            Emit(lineToRender);
        }
    };
};

// Middle operator
class CalculateLine : public dspar::Wrapper<LineToRender, LineToRender>
{
private:
    int dimensions;
    double init_a;
    double init_b;
    double step;
    int niter;

public:
    CalculateLine(
        int dimensions, double init_a,
        double init_b, double step,
        int niter)
    {
        this->dimensions = dimensions;
        this->init_b = init_b;
        this->init_a = init_a;
        this->step = step;
        this->niter = niter;
    };

    void Process(LineToRender &lineToCalculate) override
    {
        int dim = dimensions;
        char *M = new char[dimensions];
        int i = lineToCalculate.line;
        double im;
        im = init_b + (step * i);
        for (int j = 0; j < dim; j++)
        {
            double cr;
            double a = cr = init_a + step * j;
            double b = im;
            int k = 0;
            for (; k < niter; k++)
            {
                double a2 = a * a;
                double b2 = b * b;
                if ((a2 + b2) > 4.0)
                    break;
                b = 2 * a * b + im;
                a = a2 - b2 + cr;
            }
            M[j] = (unsigned char)255 - ((k * 255 / niter));
        }
        lineToCalculate.M = M;
        lineToCalculate.size = dimensions;
        Emit(lineToCalculate);
    };
};

// Sink operator
class RenderLine : public dspar::Wrapper<LineToRender, dspar::Nothing>
{
public:
    void Process(LineToRender &lineToRender)
    {
        lineToRender.size = 0;
        delete[] lineToRender.M;
        lineToRender.M = NULL;
    };
};

// Serializer for LineToRender
class LineToRenderSerializer : public dspar::SenderReceiver<LineToRender>
{
    void Send(dspar::MPISender &sender, dspar::MessageHeader &msg, LineToRender &data)
    {
        sender.SendTo(msg, data.line);
        sender.SendTo(msg, data.size);
        if (data.size > 0)
        {
            sender.SendTo(msg, data.M, data.size);
            delete[] data.M;
        }
    };
    LineToRender Receive(dspar::MPIReceiver &receiver, dspar::MessageHeader &msg)
    {
        LineToRender data;
        memset(&data, 0, sizeof(LineToRender));
        receiver.Receive(msg, &data.line);
        receiver.Receive(msg, &data.size);
        if (data.size > 0)
        {
            data.M = new char[data.size];
            receiver.Receive(msg, data.M, data.size);
        }
        return data;
    };
};

int main(int argc, char **argv)
{
    // Matrix dimensions and number of iterations
    int dim = 6000, niter = 10000;
    double init_a = -2.125, init_b = -1.5, range = 3.0;
    double step = range / (double)dim;

    // Serializers
    LineToRenderSerializer lineToRenderSerializer;

    // Operators
    GenerateWork gw(dim);
    CalculateLine cl(dim, init_a, init_b, step, niter);
    RenderLine rl;

    // Farm pattern
    auto farm = dspar::Farm(
        gw, lineToRenderSerializer,
        cl, lineToRenderSerializer,
        rl
    );

    farm.SetCollectorIsOrdered(true);
    farm.SetOnDemandScheduling(true);
    farm.SetWorkerReplicas(2);

    // Initialize the MPI environment and create the required processes dynamically
    dspar::MPIUtils mpiUtils;
    MPI_Comm comm = mpiUtils.SetTotalNumberOfProcesses(argc, argv, farm.GetTotalNumberOfProcessesNeeded() - 1);

    // Start the farm
    farm.Start(comm, 0);
    
    MPI_Finalize();
}