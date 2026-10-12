#pragma once

#include <QHash>
#include <QList>
#include <QRect>
#include <QString>

#include <optional>

// Thin wrappers over the Win32 calls FocusLock needs. Window handles are passed
// around as quintptr so <windows.h> stays out of the Qt headers.
// All rects are in physical screen pixels.
namespace winutil {

struct TargetWindow {
    quintptr handle = 0;
    QRect rect;            // visible bounds (empty when minimized)
    bool minimized = false;
};

// Running processes keyed by lower-case exe name (CreateToolhelp32Snapshot).
QHash<QString, QList<quint32>> snapshotProcesses();

// The main top-level window owned by any of the pids (EnumWindows + GetWindowRect).
// Prefers the foreground window, then the largest one.
std::optional<TargetWindow> findMainWindow(const QList<quint32>& pids);

QRect windowRect(quintptr hwnd);
QRect workArea(quintptr nearHwnd); // monitor work area; primary monitor if 0
quintptr foregroundWindow();

// Moves (and optionally resizes) a window and pins it HWND_TOPMOST without activating it.
void placeTopmost(quintptr hwnd, const QRect& rect, bool resize);

// Politely asks every top-level window of the pids to close (WM_CLOSE).
void requestClose(const QList<quint32>& pids);

// Milliseconds since boot (GetTickCount64). Monotonic, includes sleep, and
// unaffected by changes to the system clock.
quint64 tickCountMs();

// Windows' boot counter (increments every boot); 0 if unavailable.
quint32 bootId();

// FileDescription from the exe's version resource, e.g. "Google Chrome". Empty if absent.
QString fileDescription(const QString& exePath);

} // namespace winutil
