#include "ImageDisplayView.h"
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>

using namespace cnoid;

class ImageDisplayView::Impl
{
public:
    QLabel* imageLabel;

    Impl(ImageDisplayView* self) {
        imageLabel = new QLabel(self);
        QVBoxLayout* layout = new QVBoxLayout();
        layout->addWidget(imageLabel);
        self->setLayout(layout);

        // Load and display an image
        // QImage image("c:\\Users\\ai-ml-team\\Downloads\\golden-gate2.jpg");
        QImage image("c:\\Users\\ai-ml-team\\golden-gate2.jpg");
        imageLabel->setPixmap(QPixmap::fromImage(image));
    }
};

ImageDisplayView::ImageDisplayView() {
    impl = new Impl(this);
}

ImageDisplayView::~ImageDisplayView() {
    delete impl;
}

void ImageDisplayView::onActivated() {
    // This method is called when the view is activated (shown)
}

void ImageDisplayView::onDeactivated() {
    // This method is called when the view is deactivated (hidden)
}
