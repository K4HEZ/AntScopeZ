#ifndef REMOTEAPICONNECTION_H
#define REMOTEAPICONNECTION_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QVector>

class QTcpSocket;
class MainWindow;
struct RawData;
struct SParamPoint;

// One instance per accepted QTcpSocket. Owns that socket's NDJSON framing
// (readyRead() can deliver a partial line, or several lines at once -- both
// need buffering across calls) and dispatches each complete line as one
// request. Deliberately reaches AnalyzerPro directly via MainWindow, the
// same way Measurements does, rather than through a specific GUI widget --
// see the json-tcp-api plan's Context section for why (the abandoned
// OneFqWidget UDP bridge only worked while its own dialog happened to be
// open, which is the opposite of what this needs).
class RemoteApiConnection : public QObject
{
    Q_OBJECT
public:
    explicit RemoteApiConnection(QTcpSocket* socket, MainWindow* mainWindow, QObject* parent = nullptr);

signals:
    void closed(RemoteApiConnection* self);

private slots:
    void onReadyRead();
    void onDisconnected();
    // Bound once at construction to AnalyzerPro's own connect/disconnect
    // signals (not a specific GUI widget's) -- every open connection sees
    // these, not just whichever one issued a connect/disconnect command.
    void onAnalyzerFound(int index);
    void onDeviceDisconnected();
    // Same reasoning: bound to AnalyzerPro directly, seen by every open
    // connection regardless of who (if anyone) issued the sweep/stop that
    // caused them.
    void onNewMeasurement(QString name, qint64 fqFrom, qint64 fqTo, qint32 dotsNumber);
    void onNewData(RawData rawData);
    void onNewSParamPoint(SParamPoint point);
    void onMeasurementDone();

private:
    void handleLine(const QByteArray& line);
    void dispatch(const QJsonObject& request);
    void sendResponse(const QJsonObject& response, const QJsonValue& id);
    void sendError(const QJsonValue& id, const QString& message);
    void sendEvent(const QString& eventName, const QJsonObject& fields);
    QJsonObject deviceStatusObject() const;

    QJsonObject cmdStatus() const;
    QJsonObject cmdDevices() const;
    // All four either return the success-case result fields, or set
    // *error and return an empty object on failure -- dispatch() checks
    // *error to decide sendResponse() vs sendError().
    QJsonObject cmdConnect(const QJsonObject& request, QString* error);
    QJsonObject cmdDisconnect();
    QJsonObject cmdSweep(const QJsonObject& request, QString* error);
    QJsonObject cmdStop(QString* error);
    QJsonObject cmdLast() const;
    // subscribe/unsubscribe never fail -- no *error needed.
    QJsonObject cmdSubscribe(const QJsonObject& request);
    QJsonObject cmdUnsubscribe(const QJsonObject& request);

    QTcpSocket* m_socket;
    MainWindow* m_mainWindow;
    QByteArray m_lineBuffer;
    // True once this connection has sent {"cmd":"subscribe","stream":
    // "points"} -- gates whether onNewData()/onNewSParamPoint()/
    // onMeasurementDone() push to this socket. Independent of whether
    // this connection is the one that started the running sweep (or
    // whether it started via this API at all -- a GUI-triggered scan
    // streams here too, same as any other).
    bool m_subscribedToPoints = false;
    // The most recently completed (or in-progress) sweep's points, for
    // cmdLast() -- kept regardless of subscription state, cleared at
    // onNewMeasurement() (a new sweep starting).
    QVector<QJsonObject> m_lastPoints;
};

#endif // REMOTEAPICONNECTION_H
