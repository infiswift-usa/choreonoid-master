#include "OpenCVVideoStreamView.h"
#include <QLabel>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>

using namespace cnoid;

class OpenCVVideoStreamView::Impl
{
public:
    QLabel* videoLabel;
    cv::VideoCapture capture;
    QTimer timer;

    Impl(OpenCVVideoStreamView* self) {
        videoLabel = new QLabel(self);
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(videoLabel);
        self->setLayout(layout);

        // Open video stream
        capture.open("c:\\Users\\ai-ml-team\\Downloads\\Nachi Robotics_ Your Trusted Automation Partner.mp4");  // Use 0 for webcam

        // Set up a timer to update the video frame
        QObject::connect(&timer, &QTimer::timeout, [this]() {
            cv::Mat frame;
            if (capture.read(frame)) {
                // Convert the image to RGB format for display
                cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
                QImage qImage(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
                videoLabel->setPixmap(QPixmap::fromImage(qImage));
            }
        });
        timer.start(33); // Update at ~30 fps
    }

    ~Impl() {
        capture.release();
        timer.stop();
    }
};

OpenCVVideoStreamView::OpenCVVideoStreamView() {
    impl = new Impl(this);
}

OpenCVVideoStreamView::~OpenCVVideoStreamView() {
    delete impl;
}

void OpenCVVideoStreamView::onActivated() {
    // Start the timer if not already running
    if (!impl->timer.isActive())
        impl->timer.start();
}

void OpenCVVideoStreamView::onDeactivated() {
    // Stop the timer when the view is not visible
    impl->timer.stop();
}
