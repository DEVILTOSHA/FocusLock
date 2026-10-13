#pragma once

#include <QHash>
#include <QList>
#include <QRect>
#include <QString>

#include <optional>

// Thin wrappers over the macOS calls FocusLock needs (libproc, Quartz Window
// Services, AppKit). Implemented in MacUtil.mm so Objective-C stays out of the
// Qt headers. Window ids are CGWindowIDs passed as quintptr; widget handles are
// QWidget::winId() values (an NSView*).
// All rects are in Qt global coordinates (points, origin at the top-left of the
// main display), which is also what Quartz Window Services reports.
namespace macutil {

struct TargetWindow {
    quintptr handle = 0;   // CGWindowID
    QRect rect;            // bounds (empty when minimized)
    bool minimized = false; // minimized, hidden (Cmd+H) or on another Space
};

// Running processes keyed by lower-case executable name (proc_listpids + proc_pidpath).
// Only the current user's processes are visible, which is all FocusLock needs.
QHash<QString, QList<quint32>> snapshotProcesses();

// The main window owned by any of the pids (CGWindowListCopyWindowInfo).
// Prefers a window of the frontmost app, then the largest on-screen one; windows
// that are minimized or hidden still count, but lose to any visible one.
// Needs no Screen Recording permission: owner pids and bounds are always readable.
// Safe to call from any thread.
std::optional<TargetWindow> findMainWindow(const QList<quint32>& pids);

// Pid of the frontmost application, 0 if none. Call on the UI thread.
quint32 frontmostPid();

// Lifts a frameless widget's window above other apps' windows, including
// full-screen ones, on every Space, and stops it hiding when FocusLock is in the
// background. Call after the widget is shown.
void makeOverlayWindow(quintptr widgetWinId);

// Brings FocusLock to the front even if another app is active.
void activateSelf();

// Asks every app among the pids to quit (NSRunningApplication terminate), the same
// as choosing Quit from its menu, so it can still prompt to save.
void requestClose(const QList<quint32>& pids);

// Milliseconds since boot (mach_continuous_time). Monotonic, includes sleep, and
// unaffected by changes to the system clock.
quint64 tickCountMs();

// Identifies the current boot (hash of kern.bootsessionuuid); 0 if unavailable.
quint32 bootId();

// For an .app bundle: the executable that runs (CFBundleExecutable), e.g.
// "Google Chrome". Empty if the bundle can't be read.
QString bundleExecutable(const QString& bundlePath);

// For an .app bundle: the name shown to users (CFBundleDisplayName or CFBundleName).
QString bundleDisplayName(const QString& bundlePath);

} // namespace macutil
