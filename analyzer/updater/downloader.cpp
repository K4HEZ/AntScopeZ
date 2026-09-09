#include "downloader.h"

Q_LOGGING_CATEGORY(DOWNLOADER, "downloader")

extern bool g_useTls; // see mainwindow.cpp

Downloader::Downloader(QObject *parent) :
    QObject(parent),
    m_state(Finished),
    m_reply(NULL),
    m_isInfo(false)
{
    connect(&m_mng, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(fileDownloaded(QNetworkReply*)));

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
    m_timer.setSingleShot(true);
}

Downloader::~Downloader()
{

}

// Fires the actual network request AnalyzerPro::on_checkUpdatesBtn_clicked()
// builds the URL for. Re-enabled alongside that caller -- see g_useTls
// (mainwindow.cpp, "Use TLS" on Settings > Updates) and issue #14: this
// used to unconditionally disable certificate verification regardless of
// that setting; now it only does when the user has explicitly turned TLS
// off.
Downloader::State Downloader::startDownloadInfo(QUrl url)
{
    if (m_state == InProgress) {
        return m_state;
    }

    QNetworkRequest request(url);

    m_mng.clearAccessCache();
    if (!g_useTls) {
        QSslConfiguration conf = request.sslConfiguration();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(conf);
    }

    m_mng.get(request);

    qInfo() << "Firmware check: requesting" << url.toString();

    m_state = InProgress;

    m_isInfo = true;
    m_sendStatisics = false;
    return Started;
}

// Downloads the actual firmware binary once a URL for it has been found.
// Same re-enable/TLS reasoning as startDownloadInfo() above.
Downloader::State Downloader::startDownloadFw()
{
    if (m_state == InProgress) {
        return m_state;
    }

    m_isInfo = false;
    m_sendStatisics = false;

    QUrl url(m_link);

    qInfo() << "Firmware download: requesting" << url.toString();

    QNetworkRequest request(url);
    QNetworkReply *reply;

    m_mng.clearAccessCache();
    if (!g_useTls) {
        QSslConfiguration conf = request.sslConfiguration();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        request.setSslConfiguration(conf);
    }

    reply = m_mng.get(request);
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SIGNAL(progress(qint64,qint64)));

    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(progressTmr(qint64,qint64)));

    m_reply = reply;

    m_state = InProgress;

    return m_state;
}

// WARNING: Disabled due to firmware-update concerns, same as
// startDownloadInfo() above. Unlike the other two, nothing in this codebase
// currently calls this one at all -- disabled anyway, on the same "don't
// let an accidental future call phone home" reasoning.
Downloader::State Downloader::startSendStatistics(QUrl url)
{
#if 0
    if (m_state == InProgress) {
        return m_state;
    }

    QNetworkRequest request(url);
    m_mng.get(request);

    m_state = InProgress;

    m_isInfo = false;
    m_sendStatisics = true;

    return Started;
#else
    Q_UNUSED(url)
    return Finished;
#endif
}

Downloader::State Downloader::state() const
{
    return m_state;
}


void Downloader::fileDownloaded(QNetworkReply *reply)
{
    m_state = Finished;
    m_timer.stop();

    m_arr = reply->readAll();

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool ok = reply->error() == QNetworkReply::NoError;
    // errorString() reads misleadingly like a real error ("Unknown error")
    // even when error() is NoError, so only include it when there's an
    // actual error to report.
    qInfo() << "Firmware" << (m_isInfo ? "check" : "download") << "reply:"
            << reply->url().toString() << "HTTP" << httpStatus
            << (ok ? QStringLiteral("OK") : reply->errorString());

    if (reply->error() != QNetworkReply::NoError ) {
        m_lastError = reply->errorString();
        m_info.clear();
        m_link.clear();
    } else if(!m_isInfo && isHTML(m_arr)) {
        m_lastError = tr("Server does not have firmware file.");
        qInfo() << "Firmware download: server returned HTML instead of a firmware file";
    } else {
        m_lastError.clear();
        if (m_isInfo) {
            parse(&m_arr);
            if (m_ver.isEmpty())
                qInfo() << "Firmware check: HTTP OK but no <VERSION> in the response body:" << m_arr;
        }
    }

    reply->deleteLater();

    if (m_sendStatisics) {
        emit sendStatisicsComplete();
    } else {
        if (m_isInfo) {
            emit downloadInfoComplete();
        } else {
            emit downloadFileComplete();
        }
    }
    m_sendStatisics = false;
}


void Downloader::parse(QByteArray *data)
{
    QDomDocument xml;

    // QDomDocument::ParseResult and the QAnyStringView setContent() overload
    // were added in Qt 6.5; fall back to the older out-parameter form so this
    // still builds against Qt 6.2-6.4.
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QDomDocument::ParseResult result = xml.setContent(*data);

    if (!result) {
        qWarning() << result.errorMessage
                   << result.errorLine
                   << result.errorColumn;
    }
#else
    QString parseError;
    int parseErrorLine = 0;
    int parseErrorColumn = 0;

    if (!xml.setContent(*data, &parseError, &parseErrorLine, &parseErrorColumn)) {
        qWarning() << parseError
                   << parseErrorLine
                   << parseErrorColumn;
    }
#endif

    QString err;

    QDomElement docElem = xml.documentElement();
    QDomNode n = docElem.firstChild();

    m_link.clear();
    m_info.clear();
    m_ver.clear();

    while(!n.isNull())
    {
        QDomElement e = n.toElement(); // try to convert the node to an element.
        if(!e.isNull()) {
            if (e.tagName() == "INFO") {
                m_info = e.text();
            } else if (e.tagName() == "VERSION") {
                m_ver = e.text();
            } else if (e.tagName() == "LINK") {
                m_link = e.text();
            }
        }
        n = n.nextSibling();
    }
}

bool Downloader::isHTML(const QByteArray &arr)
{
    QString s(arr);
    if (s.indexOf("<html>") >= 0)
    {
        return true;
    }
    return false;
}

bool Downloader::hasDownloadLink() const
{
    if (m_link.isEmpty())
    {
        return false;
    }
    return true;
}

QString Downloader::downloadLink() const
{
    return m_link;
}


QString Downloader::version() const
{
    return m_ver;
}

QString Downloader::info() const
{
    return m_info;
}

QString Downloader::error() const
{
    return m_lastError;
}

QByteArray Downloader::file() const
{
    return m_arr;
}

void Downloader::reset()
{
    m_lastError.clear();
    m_info.clear();
    m_link.clear();
    m_arr.clear();
}

void Downloader::timeout()
{
    if (m_reply != NULL) {
        m_reply->abort();
        emit m_reply->finished();
        reset();
    }
}

void Downloader::progressTmr(qint64 dowload, qint64 total)
{
    Q_UNUSED(dowload);
    Q_UNUSED(total);
    m_timer.start(30000);
}


