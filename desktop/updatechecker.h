#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Fetches version.txt from master; ANTSCOPEZ_VERSION_URL env var overrides the URL.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Checking, Done, Failed };

    explicit UpdateChecker(QObject *parent = nullptr);

    void start();
    State state() const { return m_state; }
    QString latest() const { return m_latest; }

signals:
    void finished();

private:
    QNetworkAccessManager *m_nam;
    State m_state = State::Idle;
    QString m_latest;
};

#endif // UPDATECHECKER_H
