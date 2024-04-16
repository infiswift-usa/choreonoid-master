#include "OpenCVImageView.h"
#include <cnoid/Plugin>
#include <cnoid/ViewManager>

using namespace cnoid;

class OpenCVImageDisplayPlugin : public Plugin
{
public:
    OpenCVImageDisplayPlugin() : Plugin("OpenCVImageDisplay") {
        setUnloadable(true);
    }

    virtual bool initialize() {
        viewManager().registerClass<OpenCVImageView>(
            "OpenCVImageView", "OpenCV Image Display", ViewManager::SINGLE_OPTIONAL);
        return true;
    }
};

CNOID_IMPLEMENT_PLUGIN_ENTRY(OpenCVImageDisplayPlugin)

