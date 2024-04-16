#include "OpenCVVideoStreamView.h"
#include <cnoid/Plugin>
#include <cnoid/ViewManager>

using namespace cnoid;

class OpenCVVideoStreamPlugin : public Plugin
{
public:
    OpenCVVideoStreamPlugin() : Plugin("OpenCVVideoStream") {
        setUnloadable(true);
    }

    virtual bool initialize() {
        viewManager().registerClass<OpenCVVideoStreamView>(
            "OpenCVVideoStreamView", "OpenCV Video Stream", ViewManager::SINGLE_OPTIONAL);
        return true;
    }
};

CNOID_IMPLEMENT_PLUGIN_ENTRY(OpenCVVideoStreamPlugin)
