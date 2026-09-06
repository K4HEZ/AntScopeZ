#ifndef REMOTEAPISERVER_H
#define REMOTEAPISERVER_H

#include <QObject>
#include <QVector>
#include <QHostAddress>

class QTcpServer;
class MainWindow;
class RemoteApiConnection;

// Owns the listening socket for the in-app NDJSON remote-control API. One
// instance, owned by MainWindow. Loopback-only by default -- see the
// json-tcp-api plan for why (this is a local-control feature, not meant to
// be LAN-reachable without the user explicitly reconfiguring it).
class RemoteApiServer : public QObject
{
    Q_OBJECT
public:
    explicit RemoteApiServer(MainWindow* mainWindow, QObject* parent = nullptr);

    bool start(quint16 port, QHostAddress bindAddress = QHostAddress::LocalHost);
    void stop();
    bool isRunning() const;

private slots:
    void onNewConnection();
    void onConnectionClosed(RemoteApiConnection* conn);

private:
    MainWindow* m_mainWindow;
    QTcpServer* m_server = nullptr;
    QVector<RemoteApiConnection*> m_connections;
};

#endif // REMOTEAPISERVER_H
