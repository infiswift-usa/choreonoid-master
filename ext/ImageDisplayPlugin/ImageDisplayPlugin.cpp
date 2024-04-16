#include "ImageDisplayView.h"
#include <cnoid/Plugin>
#include <cnoid/ViewManager>

using namespace cnoid;

class ImageDisplayPlugin : public Plugin
{
public:
    ImageDisplayPlugin() : Plugin("ImageDisplay") {
        setUnloadable(true);
    }

    virtual bool initialize() {
        viewManager().registerClass<ImageDisplayView>(
            "ImageDisplayView", "Image Display", ViewManager::SINGLE_OPTIONAL);
        return true;
    }
};

CNOID_IMPLEMENT_PLUGIN_ENTRY(ImageDisplayPlugin)
