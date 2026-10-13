#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

#include <algorithm>

// One app on the watchlist, as configured by the user.
struct WatchItem {
    QString path;      // full path to the .app bundle the user picked
    QString exeName;   // bundle executable used for process matching, e.g. "Google Chrome"
    int limitSecs = 0; // session budget
};

enum class WatchState {
    Idle,       // not running
    Background, // running, but no visible window (paused)
    Active,     // running with a window; the budget is counting down
    Locked,     // budget exhausted; overlay is up until the app closes
};

// The budget refills this long after it first starts draining, not when the app closes.
// Measured on TrustedClock, so changing the system clock doesn't shorten it. The limit
// is frozen for the same period.
inline constexpr qint64 kRefillCooldownSecs = 24 * 60 * 60;

// Snapshot of one watched app, published by ProcessMonitor every tick.
struct WatchStatus {
    QString exeName;
    WatchState state = WatchState::Idle;
    int remainingSecs = 0;
    int limitSecs = 0;
    qint64 refillInSecs = 0; // time until the budget refills; 0 while untouched (limit editable)
    QList<quint32> pids;
};

Q_DECLARE_METATYPE(WatchItem)
Q_DECLARE_METATYPE(WatchStatus)

inline QString exeKey(const QString& exeName) { return exeName.toLower(); }

// "07:42" or "1:07:42"
inline QString formatClock(int secs)
{
    secs = std::max(0, secs);
    const int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    const auto pad = [](int v) { return QString::number(v).rightJustified(2, u'0'); };
    return h > 0 ? QString("%1:%2:%3").arg(h).arg(pad(m), pad(s))
                 : QString("%1:%2").arg(pad(m), pad(s));
}

// "45 min", "1 h", "1 h 30 min"
inline QString formatBudget(int secs)
{
    const int mins = std::max(0, secs) / 60, h = mins / 60, m = mins % 60;
    if (h == 0) return QString("%1 min").arg(m);
    return m == 0 ? QString("%1 h").arg(h) : QString("%1 h %2 min").arg(h).arg(m);
}

// A countdown rounded up to the minute: "21 h 4 min".
inline QString formatCountdown(qint64 secs)
{
    return secs < 60 ? QString("under a minute") : formatBudget(int(secs + 59));
}
