#include "OpenCVImageView.h"
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <opencv2/imgproc.hpp>

using namespace cnoid;

class OpenCVImageView::Impl
{
public:
    QLabel* imageLabel;
    cv::Mat image;

    Impl(OpenCVImageView* self) {
        imageLabel = new QLabel(self);
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(imageLabel);
        self->setLayout(layout);

        // Load an image using OpenCV
        image = cv::imread("c:\\Users\\ai-ml-team\\Downloads\\golden-gate2.jpg");
        // image = cv::imread("c:\\Users\\Downloads\\golden-gate2.jpg");

        // Convert the image to RGB format for display
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

        // Convert the OpenCV image to QImage
        QImage qImage(image.data, image.cols, image.rows, image.step, QImage::Format_RGB888);
        imageLabel->setPixmap(QPixmap::fromImage(qImage));
    }
};

OpenCVImageView::OpenCVImageView() {
    impl = new Impl(this);
}

OpenCVImageView::~OpenCVImageView() {
    delete impl;
}

void OpenCVImageView::onActivated() {
    // This method is called when the view is activated (shown)
}

void OpenCVImageView::onDeactivated() {
    // This method is called when the view is deactivated (hidden)
}
