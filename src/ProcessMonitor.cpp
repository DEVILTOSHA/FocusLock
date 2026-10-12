#include "ProcessMonitor.h"

#include "TrustedClock.h"
#include "WinUtil.h"

#include <QSettings>
#include <QTimer>

#include <utility>

namespace {
constexpr int kTickMs = 1000;
// Cap per-tick drain so waking from sleep/hibernate doesn't instantly lock everything.
constexpr qint64 kMaxDrainMs = 5000;
// Usage and the clock are flushed to disk this often, and on every lock/refill.
constexpr int kSaveEveryTicks = 15;
constexpr qint64 kRefillCooldownMs = kRefillCooldownSecs * 1000;
} // namespace

ProcessMonitor::ProcessMonitor(QObject* parent)
    : QObject(parent)
{
}

ProcessMonitor::~ProcessMonitor()
{
    if (m_trustedClock)
        save();
}

void ProcessMonitor::start()
{
    m_trustedClock = new TrustedClock(this);
    m_trustedClock->start();

    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::tick);
    m_clock.start();
    m_timer->start();
}

void ProcessMonitor::setWatchlist(const QList<WatchItem>& items)
{
    QList<Entry> next;
    next.reserve(items.size());
    for (const WatchItem& item : items) {
        Entry entry{item, false};
        for (const Entry& old : std::as_const(m_entries)) {
            if (exeKey(old.item.exeName) == exeKey(item.exeName)) {
                entry.locked = old.locked;
                break;
            }
        }
        next.append(entry);
    }
    m_entries = std::move(next);
}

ProcessMonitor::Usage& ProcessMonitor::usageFor(const QString& key)
{
    auto it = m_usage.find(key);
    if (it == m_usage.end()) {
        QSettings settings;
        settings.beginGroup("usage/" + key);
        Usage usage;
        usage.usedMs = settings.value("usedMs", 0).toLongLong();
        usage.windowStartMs = settings.value("windowStartMs", -1).toLongLong();
        usage.limitSecs = settings.value("limitSecs", 0).toInt();
        it = m_usage.insert(key, usage);
    }
    return *it;
}

void ProcessMonitor::save()
{
    QSettings settings;
    for (auto it = m_usage.cbegin(); it != m_usage.cend(); ++it) {
        settings.beginGroup("usage/" + it.key());
        settings.setValue("usedMs", it->usedMs);
        settings.setValue("windowStartMs", it->windowStartMs);
        settings.setValue("limitSecs", it->limitSecs);
        settings.endGroup();
    }
    m_trustedClock->save();
    m_ticksSinceSave = 0;
}

void ProcessMonitor::tick()
{
    const qint64 elapsed = std::min(m_clock.restart(), kMaxDrainMs);
    const qint64 now = m_trustedClock->nowMs();
    const auto processes = winutil::snapshotProcesses();
    bool saveNow = false;

    QList<WatchStatus> statuses;
    statuses.reserve(m_entries.size());

    for (Entry& e : m_entries) {
        const QString key = exeKey(e.item.exeName);
        const QList<quint32> pids = processes.value(key);
        const bool running = !pids.isEmpty();
        const bool hasWindow = running && winutil::findMainWindow(pids).has_value();
        Usage& usage = usageFor(key);

        // Cooldown over: refill, and lift the lock even if the app is still open.
        if (usage.inWindow() && now >= usage.windowStartMs + kRefillCooldownMs) {
            usage = Usage{};
            saveNow = true;
            if (e.locked) {
                e.locked = false;
                emit unlocked(e.item.exeName);
            }
        }

        if (e.locked) {
            if (!hasWindow) {
                e.locked = false;
                emit unlocked(e.item.exeName);
            }
        } else if (hasWindow) {
            if (!usage.inWindow()) {
                usage.windowStartMs = now;
                usage.limitSecs = e.item.limitSecs;
                saveNow = true;
            }
            const qint64 limitMs = qint64(usage.limitSecs) * 1000;
            usage.usedMs = std::min(usage.usedMs + elapsed, limitMs);
            if (usage.usedMs >= limitMs) {
                e.locked = true;
                saveNow = true;
                emit timeUp(e.item.exeName, pids,
                            (usage.windowStartMs + kRefillCooldownMs - now + 999) / 1000);
            }
        }
        // Not running, or running without a window: paused, nothing refills.

        const int limitSecs = usage.inWindow() ? usage.limitSecs : e.item.limitSecs;
        const qint64 remainingMs = std::max<qint64>(0, qint64(limitSecs) * 1000 - usage.usedMs);
        WatchStatus status;
        status.exeName = e.item.exeName;
        status.limitSecs = limitSecs;
        status.remainingSecs = int((remainingMs + 999) / 1000);
        status.refillInSecs = usage.inWindow()
            ? std::max<qint64>(1, (usage.windowStartMs + kRefillCooldownMs - now + 999) / 1000)
            : 0;
        status.pids = pids;
        status.state = e.locked   ? WatchState::Locked
                     : hasWindow  ? WatchState::Active
                     : running    ? WatchState::Background
                                  : WatchState::Idle;
        statuses.append(status);
    }

    if (saveNow || ++m_ticksSinceSave >= kSaveEveryTicks)
        save();

    emit statusUpdated(statuses);
}
