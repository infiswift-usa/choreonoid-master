#include "CameraView.h"
#include <cnoid/Plugin>
#include <cnoid/ViewManager>

using namespace cnoid;

class CameraPlugin : public Plugin
{
public:
    
    CameraPlugin() : Plugin("Camera") {
        setUnloadable(true);
    }
    
    virtual bool initialize() {
        viewManager().registerClass<CameraView>(
            "CameraView", "Camera", ViewManager::SINGLE_OPTIONAL);

        return true;
    }
};

CNOID_IMPLEMENT_PLUGIN_ENTRY(CameraPlugin)
