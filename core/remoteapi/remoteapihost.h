#ifndef REMOTEAPIHOST_H
#define REMOTEAPIHOST_H

#include <QString>

class AnalyzerPro;

// What the remote API needs from the app hosting it. The desktop's
// MainWindow implements it; a headless or mobile front end would too.
class RemoteApiHost
{
public:
    virtual ~RemoteApiHost() = default;

    virtual AnalyzerPro* analyzer() = 0;
    virtual bool isAnalyzerConnected() const = 0;
    virtual QString connectedDeviceName() const = 0;
    virtual bool isMeasuring() = 0;
    virtual void startRemoteSweep(qint64 fromHz, qint64 toHz, int points) = 0;
    virtual void stopCurrentScan() = 0;
    // Finds the named device without any UI (type: ReDeviceInfo::
    // InterfaceType) and hands it to analyzer()->on_connectDevice().
    // False if not found; AnalyzerParameters::current() is the match.
    virtual bool connectDevice(int type, const QString& name) = 0;
};

#endif // REMOTEAPIHOST_H
