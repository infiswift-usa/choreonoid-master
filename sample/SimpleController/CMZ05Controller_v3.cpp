/**
   Sample picking up motion controller for CMZ05.
   @author Shin'ichiro Nakaoka
*/

#include <cnoid/SimpleController>
#include <cnoid/JointPath>
#include <cnoid/HolderDevice>
#include <cnoid/EigenUtil>
#include "Interpolator.h"

using namespace std;
using namespace cnoid;

namespace {

const double pgain[] = {
    35000.0, 35000.0, 35000.0, 35000.0, 35000.0, 35000.0, 35000.0,
    17000.0, 17000.0 };

const double dgain[] = {
    220.0, 220.0, 220.0, 220.0, 220.0, 220.0, 220.0,
    220.0, 220.0 };

}

class CMZ05Controller : public SimpleController
{
    Body* ioBody;
    Link* ioLeftHand;
    Link* ioRightHand;
    HolderDevice* holder;
    BodyPtr ikBody;
    Link* ikWrist;
    shared_ptr<JointPath> baseToWrist;
    VectorXd qref, qold, qref_old;
    Interpolator<VectorXd> wristInterpolator;
    Interpolator<VectorXd> jointInterpolator;
    int phase;
    double time;
    double timeStep;
    double dq_hand;
    bool isKinematicsMode;

public:
    virtual bool initialize(SimpleControllerIO* io) override;
    virtual bool control() override;
    bool checkForHolding();
    bool checkForReleasing();
};

CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(CMZ05Controller)


bool CMZ05Controller::initialize(SimpleControllerIO* io)
{
    ioBody = io->body();
    
    ioLeftHand  = ioBody->link("link_j5");
    ioRightHand = ioBody->link("link_j6");
    
    isKinematicsMode = false;
    for(auto opt : io->options()){
        if(opt == "kinematics"){
            isKinematicsMode = true;
            holder = ioBody->findDevice<HolderDevice>();
        }
    }
        
    ikBody = ioBody->clone();
    ikWrist = ikBody->link("link_j4");
    Link* base = ikBody->rootLink();
    baseToWrist = JointPath::getCustomPath(base, ikWrist);
    base->p().setZero();
    base->R().setIdentity();
    
    const int nj = ioBody->numJoints();
    qold.resize(nj);
    for(int i=0; i < nj; ++i){
        Link* joint = ioBody->joint(i);
        joint->setActuationMode(isKinematicsMode ? JointDisplacement : JointEffort);
        io->enableIO(joint);
        double q = joint->q();
        ikBody->joint(i)->q() = q;
        qold[i] = q;
    }
    
    baseToWrist->calcForwardKinematics();
    qref = qold;
    qref_old = qold;
    
    VectorXd p0(6);
    p0.head<3>() = ikWrist->p();
    p0.tail<3>() = rpyFromRot(ikWrist->R());
    
    VectorXd p1(6);
    p1.head<3>() = Vector3(0.9, 0.0, 0.25);
    p1.tail<3>() = radian(Vector3(180.0, 0.0, 0.0));
    
    wristInterpolator.clear();
    wristInterpolator.appendSample(0.0, p0);
    wristInterpolator.appendSample(1.0, p1);
    p1.z() = 0.2;
    wristInterpolator.appendSample(1.2, p1);
    wristInterpolator.update();
    
    phase = 0;
    time = 0.0;
    timeStep = io->timeStep();
    dq_hand = 0.0;
    
    return true;
}


bool CMZ05Controller::control()
{
    bool isActive = true;
    
    VectorXd p(6);
    
    if(phase <= 3){
        p = wristInterpolator.interpolate(time);
        Isometry3 T;
        T.linear() = rotFromRpy(Vector3(p.tail<3>()));
        T.translation() = p.head<3>();
        if(baseToWrist->calcInverseKinematics(T)){
            for(int i=0; i < baseToWrist->numJoints(); ++i){
                Link* joint = baseToWrist->joint(i);
                qref[joint->jointId()] = joint->q();
            }
        }
    }
    
    if(phase == 0){
        if(time > wristInterpolator.domainUpper()){
            phase = 1;
        }
        
    } else if(phase == 1){
        if(!checkForHolding()){
            dq_hand = std::min(dq_hand + 0.00001, 0.0005);
            qref[ioRightHand->jointId()] -= radian(dq_hand);
            qref[ioLeftHand->jointId()]  += radian(dq_hand);
            
        } else {
            VectorXd p2(6);
            p2.head<3>() = Vector3(0.0, 0.5, 1.0);
            p2.tail<3>() = radian(Vector3(180.0, -45, 90.0));
            
            VectorXd p3(6);
            p3.head<3>() = Vector3(0.0, 0.7, isKinematicsMode ? 0.501 : 0.52);
            p3.tail<3>() = radian(Vector3(180.0, 0, 90.0));
            
            wristInterpolator.clear();
            wristInterpolator.appendSample(time, p);
            wristInterpolator.appendSample(time + 1.0, p2);
            wristInterpolator.appendSample(time + 1.5, p3);
            wristInterpolator.appendSample(time + 1.7, p3);
            wristInterpolator.update();
            phase = 2;
        }
    } else if(phase == 2){
        if(time > wristInterpolator.domainUpper()){
            phase = 3;
            dq_hand = 0.0;
        }
    } else if(phase == 3){
        if(!checkForReleasing()){
            dq_hand = std::min(dq_hand + 0.00001, 0.002);
            qref[ioRightHand->jointId()] += radian(dq_hand);
            qref[ioLeftHand->jointId()]  -= radian(dq_hand);
        } else {
            jointInterpolator.clear();
            jointInterpolator.appendSample(time, qref);
            VectorXd qf = VectorXd::Zero(qref.size());
            qf[ioRightHand->jointId()] = qref[ioRightHand->jointId()];
            qf[ioLeftHand->jointId()]  = qref[ioLeftHand->jointId()];
            jointInterpolator.appendSample(time + 1.0, qf);
            jointInterpolator.update();
            phase = 4;
        }
    } else if(phase == 4){
        qref = jointInterpolator.interpolate(time);
        if(time > jointInterpolator.domainUpper()){
            isActive = false;
        }
    }

    for(int i=0; i < ioBody->numJoints(); ++i){
        if(isKinematicsMode){
            ioBody->joint(i)->q_target() = qref[i];
        } else {
            double q = ioBody->joint(i)->q();
            double dq = (q - qold[i]) / timeStep;
            double dq_ref = (qref[i] - qref_old[i]) / timeStep;
            ioBody->joint(i)->u() = (qref[i] - q) * pgain[i] + (dq_ref - dq) * dgain[i];
            qold[i] = q;
        }
    }
    qref_old = qref;
    time += timeStep;
    
    return isActive;
}


bool CMZ05Controller::checkForHolding()
{
    bool isHolding = false;
    
    if(!isKinematicsMode){
        isHolding = fabs(ioRightHand->u()) >= 40.0 && fabs(ioLeftHand->u()) >= 40.0;
    } else {
        if(ioRightHand->q() < 0.015 && ioLeftHand->q() > -0.015){
            isHolding = true;
            if(holder){
                holder->on(true);
                holder->notifyStateChange();
            }
        }
    }

    return isHolding;
}


bool CMZ05Controller::checkForReleasing()
{
    bool isReleasing = qref[ioRightHand->jointId()] >= 0.028 && qref[ioLeftHand->jointId()] <= -0.028;

    if(isReleasing && isKinematicsMode && holder){
        holder->on(false);
        holder->notifyStateChange();
    }

    return isReleasing;
}
