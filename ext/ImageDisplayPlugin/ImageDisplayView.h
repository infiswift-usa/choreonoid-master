#ifndef CNOID_IMAGEDISPLAYVIEW_H
#define CNOID_IMAGEDISPLAYVIEW_H

#include <cnoid/View>

namespace cnoid {

class ImageDisplayView : public View
{
public:
    ImageDisplayView();
    virtual ~ImageDisplayView();

protected:
    virtual void onActivated() override;
    virtual void onDeactivated() override;

private:
    class Impl;
    Impl* impl;
};

}

#endif // CNOID_IMAGEDISPLAYVIEW_H
