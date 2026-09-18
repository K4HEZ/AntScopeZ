#include "updatechecker.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

extern bool g_useTls; // see mainwindow.cpp

UpdateChecker::UpdateChecker(QObject *parent) :
    QObject(parent),
    m_nam(new QNetworkAccessManager(this))
{
}

void UpdateChecker::start()
{
    if (m_state == State::Checking) {
        return;
    }

    QString url = qEnvironmentVariable("ANTSCOPEZ_VERSION_URL");
    if (url.isEmpty()) {
        url = QStringLiteral("https://raw.githubusercontent.com/K4HEZ/AntScopeZ/master/version.txt");
    }
    if (!g_useTls) {
        url.replace(QStringLiteral("https://"), QStringLiteral("http://"));
    }

    m_state = State::Checking;
    QNetworkRequest req{QUrl(url)};
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        // Anything but a bare x.y.z (error page, captive portal) counts as failed.
        const QString text = QString::fromUtf8(reply->readAll()).trimmed();
        static const QRegularExpression re(QStringLiteral("^\\d+\\.\\d+\\.\\d+$"));
        if (reply->error() == QNetworkReply::NoError && re.match(text).hasMatch()) {
            m_latest = text;
            m_state = State::Done;
        } else {
            m_state = State::Failed;
        }
        emit finished();
    });
}
