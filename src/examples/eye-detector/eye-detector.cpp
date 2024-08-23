#include "dspar/farm/farm.h"
#include <opencv2/opencv.hpp>
#include "common.hpp"
// #include "matcerealisation.hpp"

// Source operator
class Source : public dspar::Emitter<cv::Mat>
{
private:
    cv::VideoCapture videoIn;

public:
    void Start()
    {
        // Open input video
        std::string inputFile = "workloads/eye_detector/several_faces_15s.mp4";
        videoIn = cv::VideoCapture(inputFile, cv::CAP_FFMPEG);
        if (!videoIn.isOpened())
        {
            printf("Unable to open input video %s\n", inputFile.c_str());
            exit(1);
        }
    }

    void End()
    {
        // Release resources
        videoIn.release();
    }

    void Produce()
    {
        while (true)
        {
            // Read frame
            auto frame = cv::Mat();
            videoIn >> frame;
            if (frame.empty())
                break;

            Emit(frame);
        }
    };
};

// Middle operator
class Middle : public dspar::Worker<cv::Mat, cv::Mat>
{
private:
    cv::CascadeClassifier faceDetector;
    cv::CascadeClassifier eyeDetector;

public:
    void Start()
    {
        faceDetector = cv::CascadeClassifier("workloads/eye_detector/haarcascade_frontalface_alt.xml");
        eyeDetector = cv::CascadeClassifier("workloads/eye_detector/haarcascade_eye.xml");
    }

    void Process(cv::Mat &frame)
    {
        // Convert to gray and equalize frame
        auto equalized = prepareFrame(frame);

        // Detect faces
        auto faces = detectFaces(equalized, faceDetector);

        for (auto face : faces)
        {
            // Detect eyes
            auto eyes = detectEyes(equalized, face, eyeDetector);

            // Draw face and eyes
            drawInFrame(frame, eyes, face);
        }

        Emit(frame);
    };
};

// Sink operator
class Sink : public dspar::Collector<cv::Mat>
{
private:
    cv::VideoWriter videoOut;

public:
    void Start()
    {
        // Open output video
        std::string outputFile = "workloads/eye_detector/output.avi";
        auto fourcc = cv::VideoWriter::fourcc('m', 'p', 'g', '1');
        videoOut = cv::VideoWriter(outputFile, fourcc, 30, cv::Size(640, 360), true);
        if (!videoOut.isOpened())
        {
            printf("Unable to open output video %s\n", outputFile.c_str());
            exit(1);
        }
    }

    void End()
    {
        // Release resources
        videoOut.release();
    }

    void Process(cv::Mat &frame)
    {
        // Write output frame
        videoOut.write(frame);
    };
};

// Serializer for opencv Mat
class MatSerializer : public dspar::SenderReceiver<cv::Mat>
{
    void Send(dspar::MPISender &sender, dspar::MessageHeader &msg, cv::Mat &data)
    {
        int type = data.type();
        bool continuous = data.isContinuous();

        sender.SendTo(msg, data.rows);
        sender.SendTo(msg, data.cols);
        sender.SendTo(msg, type);
        sender.SendTo(msg, continuous);

        if (continuous)
        {
            const int data_size = data.rows * data.cols * static_cast<int>(data.elemSize());
            sender.SendTo(msg, data.ptr(), data_size);
        }
        else
        {
            const int row_size = data.cols * static_cast<int>(data.elemSize());
            for (int i = 0; i < data.rows; i++)
                sender.SendTo(msg, data.ptr(i), row_size);
        }
    };

    cv::Mat Receive(dspar::MPIReceiver &receiver, dspar::MessageHeader &msg)
    {
        int rows, cols, type;
        bool continuous;

        receiver.Receive(msg, &rows);
        receiver.Receive(msg, &cols);
        receiver.Receive(msg, &type);
        receiver.Receive(msg, &continuous);

        cv::Mat data(rows, cols, type);
        if (continuous)
        {
            const int data_size = rows * cols * static_cast<int>(data.elemSize());
            receiver.Receive(msg, data.ptr(), data_size);
        }
        else
        {
            const int row_size = cols * static_cast<int>(data.elemSize());
            for (int i = 0; i < rows; i++)
                receiver.Receive(msg, data.ptr(i), row_size);
        }

        return data;
    };
};

int main(int argc, char **argv)
{
    // Serializers
    MatSerializer matSerializer;

    // Operators
    Source source;
    Middle middle;
    Sink sink;

    // Farm
    auto farm = dspar::Farm(
        source, matSerializer,
        middle, matSerializer,
        sink);

    farm.SetCollectorIsOrdered(true);
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