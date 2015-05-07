#ifndef QMLOCKS_STUB
#define QMLOCKS_STUB

#include <stubbase.h>
#include <qmlocks.h>

// 1. DECLARE STUB
// FIXME - stubgen is not yet finished
class QmLocksStub : public StubBase {
public:
    virtual void QmLocksConstructor(QObject *parent = 0);
    virtual void QmLocksDestructor();
    virtual MeeGo::QmLocks::State getState(MeeGo::QmLocks::Lock what) const;
    virtual bool setState(MeeGo::QmLocks::Lock what, MeeGo::QmLocks::State how);
    virtual void connectNotify(const QMetaMethod &signal);
    virtual void disconnectNotify(const QMetaMethod &signal);
};

// 2. IMPLEMENT STUB
void QmLocksStub::QmLocksConstructor(QObject *parent) {
    Q_UNUSED(parent)
}

void QmLocksStub::QmLocksDestructor() {

}

MeeGo::QmLocks::State QmLocksStub::getState(MeeGo::QmLocks::Lock what) const {
    QList<ParameterBase*> params;
    params.append(new Parameter<MeeGo::QmLocks::Lock>(what));
    stubMethodEntered("getState", params);
    return stubReturnValue<MeeGo::QmLocks::State>("getState");
}

bool QmLocksStub::setState(MeeGo::QmLocks::Lock what, MeeGo::QmLocks::State how) {
    QList<ParameterBase*> params;
    params.append(new Parameter<MeeGo::QmLocks::Lock>(what));
    params.append(new Parameter<MeeGo::QmLocks::State>(how));
    stubMethodEntered("setState", params);
    return stubReturnValue<bool>("setState");
}

void QmLocksStub::connectNotify(const QMetaMethod &signal)
{
  QList<ParameterBase*> params;
  params.append(new Parameter<const QMetaMethod &>(signal));
  stubMethodEntered("connectNotify",params);
}

void QmLocksStub::disconnectNotify(const QMetaMethod &signal)
{
  QList<ParameterBase*> params;
  params.append(new Parameter<const QMetaMethod &>(signal));
  stubMethodEntered("disconnectNotify",params);
}

// 3. CREATE A STUB INSTANCE
QmLocksStub gDefaultQmLocksStub;
QmLocksStub* gQmLocksStub = &gDefaultQmLocksStub;

// 4. CREATE A PROXY WHICH CALLS THE STUB
namespace MeeGo
{

QmLocks::QmLocks(QObject *parent) {
    gQmLocksStub->QmLocksConstructor(parent);
}

QmLocks::~QmLocks() {
    gQmLocksStub->QmLocksDestructor();
}

QmLocks::State QmLocks::getState(QmLocks::Lock what) const {
    return gQmLocksStub->getState(what);
}

bool QmLocks::setState(QmLocks::Lock what, QmLocks::State how) {
    return gQmLocksStub->setState(what, how);
}

void QmLocks::connectNotify(const QMetaMethod &signal)
{
    gQmLocksStub->connectNotify(signal);
}

void QmLocks::disconnectNotify(const QMetaMethod &signal)
{
    gQmLocksStub->disconnectNotify(signal);
}

}
#endif // QMLOCKS_STUB
