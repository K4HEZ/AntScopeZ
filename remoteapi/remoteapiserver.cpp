#include "remoteapiserver.h"
#include "remoteapiconnection.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <qdebug.h>

RemoteApiServer::RemoteApiServer(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

bool RemoteApiServer::start(quint16 port, QHostAddress bindAddress)
{
    if (m_server != nullptr)
        stop();

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &RemoteApiServer::onNewConnection);

    if (!m_server->listen(bindAddress, port)) {
        qDebug() << "RemoteApiServer: failed to listen on" << bindAddress.toString() << port
                  << "-" << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    return true;
}

void RemoteApiServer::stop()
{
    if (m_server == nullptr)
        return;

    // Disconnect each connection's closed() signal before scheduling its
    // deletion: closing/destroying its QTcpSocket may itself emit
    // disconnected() (Qt's docs are non-committal on whether close() does
    // this synchronously), which would otherwise re-enter
    // onConnectionClosed() mid-teardown and mutate m_connections while
    // it's being iterated. deleteLater() (not delete) so nothing runs
    // until back on the event loop, well after this function returns.
    QVector<RemoteApiConnection*> conns = m_connections;
    m_connections.clear();
    foreach (RemoteApiConnection* conn, conns) {
        conn->disconnect(this);
        conn->deleteLater();
    }

    m_server->close();
    delete m_server;
    m_server = nullptr;
}

bool RemoteApiServer::isRunning() const
{
    return m_server != nullptr && m_server->isListening();
}

void RemoteApiServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        RemoteApiConnection* conn = new RemoteApiConnection(socket, m_mainWindow, this);
        connect(conn, &RemoteApiConnection::closed, this, &RemoteApiServer::onConnectionClosed);
        m_connections.append(conn);
    }
}

void RemoteApiServer::onConnectionClosed(RemoteApiConnection* conn)
{
    m_connections.removeOne(conn);
    conn->deleteLater();
}
