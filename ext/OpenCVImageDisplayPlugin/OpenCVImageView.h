#ifndef CNOID_OPENCVIMAGEDISPLAYVIEW_H
#define CNOID_OPENCVIMAGEDISPLAYVIEW_H

#include <cnoid/View>
#include <opencv2/opencv.hpp>

namespace cnoid {

class OpenCVImageView : public View
{
public:
    OpenCVImageView();
    virtual ~OpenCVImageView();

protected:
    virtual void onActivated() override;
    virtual void onDeactivated() override;

private:
    class Impl;
    Impl* impl;
};

}

#endif // CNOID_OPENCVIMAGEDISPLAYVIEW_H

