#pragma once

#include <QObject>
#include <QTimer>

class QNetworkAccessManager;

// FocusLock's own time line for budget cooldowns, independent of the system clock,
// so moving the Mac clock forward can't make budgets refill early.
//
//  - Within a boot it advances with CLOCK_MONOTONIC_RAW (monotonic, includes sleep,
//    unaffected by clock changes), including while FocusLock itself was closed.
//  - Time across reboots can't be measured locally, so it's credited from network
//    time: the Date header of well-known HTTPS servers, compared against the last
//    sync point. Until a sync succeeds nothing is credited, so the clock can run
//    slow but never fast.
//
// Lives on the monitor thread. State persists in QSettings under "clock".
class TrustedClock : public QObject {
    Q_OBJECT

public:
    explicit TrustedClock(QObject* parent = nullptr);
    ~TrustedClock() override;

    void start();
    qint64 nowMs();
    void save();

private:
    void requestNetworkTime();
    void onNetworkTime(qint64 epochSecs);

    QNetworkAccessManager* m_net = nullptr;
    QTimer m_syncTimer;
    qsizetype m_source = 0;
    bool m_started = false;

    qint64 m_nowMs = 0;
    quint64 m_lastTick = 0;
    quint32 m_bootId = 0;

    bool m_hasSync = false;
    qint64 m_syncEpoch = 0; // network time (epoch secs) at the last sync
    qint64 m_syncNowMs = 0; // our time at the last sync
};
