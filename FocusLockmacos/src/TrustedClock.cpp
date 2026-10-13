#include "TrustedClock.h"

#include "MacUtil.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include <iterator>

namespace {

constexpr int kSyncEveryMs = 30 * 60 * 1000;
constexpr int kRetryMs = 2 * 60 * 1000;
constexpr int kRequestTimeoutMs = 10000;
// The Date header has 1 s resolution; ignore differences below this.
constexpr qint64 kCreditToleranceMs = 2000;

const char* const kTimeSources[] = {
    "https://www.google.com/",
    "https://www.cloudflare.com/",
    "https://www.apple.com/",
};

// "Sun, 11 Oct 2026 03:41:01 GMT" -> epoch seconds, 0 if unparseable.
qint64 parseHttpDate(const QByteArray& raw)
{
    QString text = QString::fromLatin1(raw).trimmed();
    if (text.endsWith(" GMT"))
        text = text.chopped(4) + " +0000";
    const QDateTime dt = QDateTime::fromString(text, Qt::RFC2822Date);
    return dt.isValid() ? dt.toSecsSinceEpoch() : 0;
}

} // namespace

TrustedClock::TrustedClock(QObject* parent)
    : QObject(parent)
{
    m_syncTimer.setSingleShot(true);
    connect(&m_syncTimer, &QTimer::timeout, this, &TrustedClock::requestNetworkTime);
}

TrustedClock::~TrustedClock()
{
    if (m_started)
        save();
}

void TrustedClock::start()
{
    QSettings settings;
    settings.beginGroup("clock");
    m_nowMs = settings.value("nowMs", 0).toLongLong();
    const quint64 savedTick = settings.value("tick", 0).toULongLong();
    const quint32 savedBoot = settings.value("bootId", 0).toUInt();
    m_hasSync = settings.contains("syncEpoch");
    m_syncEpoch = settings.value("syncEpoch", 0).toLongLong();
    m_syncNowMs = settings.value("syncNowMs", 0).toLongLong();
    settings.endGroup();

    m_bootId = macutil::bootId();
    const quint64 tick = macutil::tickCountMs();

    // Same boot: the tick counter covers the time FocusLock was closed.
    // New boot: the gap is unknown until a network sync credits it.
    const bool sameBoot = savedTick != 0 && tick >= savedTick
                       && (m_bootId == 0 || savedBoot == m_bootId);
    if (sameBoot)
        m_nowMs += qint64(tick - savedTick);
    m_lastTick = tick;
    m_started = true;
    save();

    m_net = new QNetworkAccessManager(this);
    requestNetworkTime();
}

qint64 TrustedClock::nowMs()
{
    const quint64 tick = macutil::tickCountMs();
    if (tick > m_lastTick)
        m_nowMs += qint64(tick - m_lastTick);
    m_lastTick = tick;
    return m_nowMs;
}

void TrustedClock::save()
{
    nowMs();
    QSettings settings;
    settings.beginGroup("clock");
    settings.setValue("nowMs", m_nowMs);
    settings.setValue("tick", m_lastTick);
    settings.setValue("bootId", m_bootId);
    if (m_hasSync) {
        settings.setValue("syncEpoch", m_syncEpoch);
        settings.setValue("syncNowMs", m_syncNowMs);
    }
    settings.endGroup();
}

void TrustedClock::requestNetworkTime()
{
    QNetworkRequest request(QUrl(QString::fromLatin1(kTimeSources[m_source])));
    request.setTransferTimeout(kRequestTimeoutMs);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkReply* reply = m_net->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        // Any response that made it through TLS carries a trustworthy Date, even a 3xx/4xx.
        const qint64 epoch = parseHttpDate(reply->rawHeader("Date"));
        if (epoch > 0) {
            onNetworkTime(epoch);
            m_syncTimer.start(kSyncEveryMs);
            return;
        }
        m_source = (m_source + 1) % qsizetype(std::size(kTimeSources));
        m_syncTimer.start(m_source == 0 ? kRetryMs : 0);
    });
}

void TrustedClock::onNetworkTime(qint64 epochSecs)
{
    const qint64 now = nowMs();
    if (m_hasSync) {
        // Real time that passed since the last sync, versus what we counted.
        // Anything missing (a reboot, mostly) is credited; we never go backwards.
        const qint64 realMs = (epochSecs - m_syncEpoch) * 1000;
        const qint64 countedMs = now - m_syncNowMs;
        if (realMs - countedMs > kCreditToleranceMs)
            m_nowMs += realMs - countedMs;
    }
    m_hasSync = true;
    m_syncEpoch = epochSecs;
    m_syncNowMs = m_nowMs;
    save();
}
