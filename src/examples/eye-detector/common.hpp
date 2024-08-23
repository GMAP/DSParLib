#ifndef COMMON_EYE_DETECTOR_H
#define COMMON_EYE_DETECTOR_H

#include <opencv2/opencv.hpp>

// Convert the input frame to a gray equalized frame
cv::Mat prepareFrame(cv::Mat &frame)
{
    auto gray = cv::Mat();
    auto equalized = cv::Mat();
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY, 0);
    cv::equalizeHist(gray, equalized);
    return equalized;
}

// Detect faces in the input frame
std::vector<cv::Rect> detectFaces(cv::Mat &frame, cv::CascadeClassifier &faceDetector)
{
    std::vector<cv::Rect> faces;
    faceDetector.detectMultiScale(
        frame,
        faces,
        1.01,
        40,
        0,
        cv::Size(frame.size().width * 0.06, frame.size().height * 0.06),
        cv::Size(frame.size().width * 0.18, frame.size().height * 0.18)
    );

    return faces;
}

// Detect eyes in the input frame
std::vector<cv::Rect> detectEyes(cv::Mat &equalized, cv::Rect &face, cv::CascadeClassifier &eyeDetector)
{
    cv::Mat frame = cv::Mat(equalized, face);
    std::vector<cv::Rect> eyes;
    eyeDetector.detectMultiScale(
        frame,
        eyes,
        1.01,
        40,
        cv::CASCADE_SCALE_IMAGE,
        cv::Size(frame.size().width * 0.06, frame.size().height * 0.06),
        cv::Size(frame.size().width * 0.18, frame.size().height * 0.18)
    );

    return eyes;
}

// Draw the face and eyes in the input frame
void drawInFrame(cv::Mat &frame, std::vector<cv::Rect> &eyes, cv::Rect &face)
{
    auto scaledFace = cv::Rect{
        face.x,
        face.y,
        face.width,
        face.height
    };

    cv::rectangle(
        frame,
        scaledFace,
        cv::Scalar(255, 0, 0),  // Color
        1,                      // Thickness
        8,                      // Line type
        0                       // Shift
    );

    // Draw eyes
    // if (eyes.size() == 2)  // Only draw if both eyes are detected
    // {
    for (auto eye : eyes)
    {
        cv::rectangle(
            frame,
            cv::Rect{
                face.tl().x + eye.tl().x,
                face.tl().y + eye.tl().y,
                eye.width,
                eye.height
            },
            cv::Scalar(0, 255, 0),  // Color
            1,                      // Thickness
            8,                      // Line type
            0                       // Shift
        );
    }
    // }
}

#endif