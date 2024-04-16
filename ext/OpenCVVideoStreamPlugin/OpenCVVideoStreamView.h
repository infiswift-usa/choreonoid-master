#ifndef CNOID_OPENCVVIDEOSTREAMVIEW_H
#define CNOID_OPENCVVIDEOSTREAMVIEW_H

#include <cnoid/View>
#include <opencv2/opencv.hpp>
#include <QTimer>

namespace cnoid {

class OpenCVVideoStreamView : public View
{
public:
    OpenCVVideoStreamView();
    virtual ~OpenCVVideoStreamView();

protected:
    virtual void onActivated() override;
    virtual void onDeactivated() override;

private:
    class Impl;
    Impl* impl;
};

}

#endif // CNOID_OPENCVVIDEOSTREAMVIEW_H
