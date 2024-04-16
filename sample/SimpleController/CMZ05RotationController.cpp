#include <cnoid/SimpleController>
#include <vector>

using namespace cnoid;

class CMZ05CombinedController : public SimpleController {
    BodyPtr ioBody;
    double timeStep;
    std::vector<double> qref;
    std::vector<double> qold;
    std::vector<double> pgain;
    std::vector<double> dgain;

public:
    virtual bool initialize(SimpleControllerIO* io) override {
        ioBody = io->body();
        timeStep = io->timeStep();

        // Initialize proportional and derivative gains for each joint
        pgain = {2000.0, 2000.0, 8000.0, 8000.0, 8000.0, 8000.0};
        dgain = {300.0, 200.0, 100.0, 100.0, 100.0, 100.0};

        for (int i = 0; i < ioBody->numJoints(); ++i) {
            Link* joint = ioBody->joint(i);
            joint->setActuationMode(Link::JOINT_TORQUE);
            io->enableIO(joint);
            qref.push_back(joint->q());
        }
        qold = qref;

        // Set the target angles for link_j1 and link_j2 to 30 degrees (converted to radians)
        qref[0] = 70.0 * M_PI / 180.0; // for link_j1
        qref[2] = -30.0 * M_PI / 180.0; // for link_j3

        return true;
    }

    virtual bool control() override {
        for (int i = 0; i < ioBody->numJoints(); ++i) {
            Link* joint = ioBody->joint(i);
            double q = joint->q();
            double dq = (q - qold[i]) / timeStep;
            double u = (qref[i] - q) * pgain[i] + (0.0 - dq) * dgain[i];
            qold[i] = q;
            joint->u() = u;
        }
        return true;
    }
};

CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(CMZ05CombinedController)
