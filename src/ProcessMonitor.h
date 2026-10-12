#pragma once

#include "WatchTypes.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>

class QTimer;
class TrustedClock;

// Lives on a worker thread. Once a second it snapshots the process list,
// counts down the budget of every watched app that has a visible window,
// and reports state changes back to the UI thread via queued signals.
//
// Budget rules:
//  - The budget only drains while the app has a visible top-level window.
//  - It refills kRefillCooldownSecs after it first started draining, measured on
//    TrustedClock rather than the system clock. Closing the app does not refill it,
//    so reopening a spent app locks it again immediately.
//  - For that same window the limit is frozen at its value when draining started;
//    later edits to the watchlist don't apply until the refill.
//  - The lock lifts when the process exits or its last window closes, or when
//    the budget refills.
//  - Usage is persisted per exe name, so restarting FocusLock or removing and
//    re-adding an app doesn't reset it.
class ProcessMonitor : public QObject {
    Q_OBJECT

public:
    explicit ProcessMonitor(QObject* parent = nullptr);
    ~ProcessMonitor() override;

public slots:
    void start();
    void setWatchlist(const QList<WatchItem>& items);

signals:
    void statusUpdated(const QList<WatchStatus>& statuses);
    void timeUp(const QString& exeName, const QList<quint32>& pids, qint64 refillInSecs);
    void unlocked(const QString& exeName);

private:
    struct Usage {
        qint64 usedMs = 0;
        qint64 windowStartMs = -1; // TrustedClock time of the first drain; -1 = full budget
        int limitSecs = 0;         // limit frozen for this window
        bool inWindow() const { return windowStartMs >= 0; }
    };

    struct Entry {
        WatchItem item;
        bool locked = false;
    };

    void tick();
    Usage& usageFor(const QString& key);
    void save();

    QList<Entry> m_entries;
    QHash<QString, Usage> m_usage; // keyed by exeKey(); outlives watchlist removals
    TrustedClock* m_trustedClock = nullptr;
    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    int m_ticksSinceSave = 0;
};
