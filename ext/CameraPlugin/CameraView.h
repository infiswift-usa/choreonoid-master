#ifndef CNOID_CAMERA_PLUGIN_CAMERA_VIEW_H
#define CNOID_CAMERA_PLUGIN_CAMERA_VIEW_H

#include <cnoid/View>
#include <cnoid/TimeBar>
#include <QCamera>
#include <QCameraViewfinder>

namespace cnoid {

class CameraScene : public QCameraViewfinder
{
public:
    CameraScene(QWidget* parent = 0);
    bool setTime(double time);

protected:
    virtual void initializeCamera();
    virtual void startCamera();
    virtual void stopCamera();
    
private:
    QCamera* camera;
    QCameraViewfinder* viewfinder;
    // Additional camera-related members
};

class CameraView : public View
{
public:
    CameraView();

protected:
    virtual void onActivated();
    virtual void onDeactivated();
    
private:
    CameraScene* cameraScene;
    cnoid::TimeBar* timeBar;
    Connection timeChangeConnection;
};

} // namespace cnoid

#endif // CNOID_CAMERA_PLUGIN_CAMERA_VIEW_H
