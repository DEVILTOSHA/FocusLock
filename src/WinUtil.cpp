#include "WinUtil.h"

#include <QSet>

#include <windows.h>
#include <dwmapi.h>
#include <tlhelp32.h>

#include <functional>
#include <limits>

namespace winutil {

namespace {

QRect toQRect(const RECT& r) { return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top); }

bool isCloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        && cloaked != 0;
}

// GetWindowRect includes the invisible resize borders on Windows 10+, so refine
// it with the DWM frame bounds when they're available.
QRect visibleBounds(HWND hwnd)
{
    RECT r{};
    GetWindowRect(hwnd, &r);
    RECT frame{};
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame))))
        r = frame;
    return toQRect(r);
}

bool isAppWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
        return false;
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return false;
    return !isCloaked(hwnd);
}

using WindowVisitor = std::function<void(HWND)>;

struct EnumContext {
    QSet<quint32> pids;
    const WindowVisitor* visit;
};

BOOL CALLBACK enumProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<EnumContext*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (ctx->pids.contains(pid) && isAppWindow(hwnd))
        (*ctx->visit)(hwnd);
    return TRUE;
}

void forEachAppWindow(const QList<quint32>& pids, const WindowVisitor& visit)
{
    if (pids.isEmpty())
        return;
    EnumContext ctx{QSet<quint32>(pids.begin(), pids.end()), &visit};
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&ctx));
}

} // namespace

QHash<QString, QList<quint32>> snapshotProcesses()
{
    QHash<QString, QList<quint32>> result;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return result;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            result[QString::fromWCharArray(entry.szExeFile).toLower()].append(entry.th32ProcessID);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return result;
}

std::optional<TargetWindow> findMainWindow(const QList<quint32>& pids)
{
    const HWND foreground = GetForegroundWindow();
    HWND best = nullptr;
    qint64 bestScore = -1;

    forEachAppWindow(pids, [&](HWND hwnd) {
        qint64 score = 0; // minimized windows still count, but lose to any visible one
        if (hwnd == foreground) {
            score = std::numeric_limits<qint64>::max();
        } else if (!IsIconic(hwnd)) {
            const QRect r = visibleBounds(hwnd);
            score = qint64(r.width()) * r.height();
            if (score <= 0)
                return; // zero-size helper windows
        }
        if (score > bestScore) {
            best = hwnd;
            bestScore = score;
        }
    });

    if (!best)
        return std::nullopt;

    TargetWindow target;
    target.handle = reinterpret_cast<quintptr>(best);
    target.minimized = IsIconic(best);
    if (!target.minimized)
        target.rect = visibleBounds(best);
    return target;
}

QRect windowRect(quintptr hwnd)
{
    RECT r{};
    GetWindowRect(reinterpret_cast<HWND>(hwnd), &r);
    return toQRect(r);
}

QRect workArea(quintptr nearHwnd)
{
    const HMONITOR monitor = nearHwnd
        ? MonitorFromWindow(reinterpret_cast<HWND>(nearHwnd), MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    return toQRect(info.rcWork);
}

quintptr foregroundWindow()
{
    return reinterpret_cast<quintptr>(GetForegroundWindow());
}

void placeTopmost(quintptr hwnd, const QRect& rect, bool resize)
{
    UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    if (!resize)
        flags |= SWP_NOSIZE;
    SetWindowPos(reinterpret_cast<HWND>(hwnd), HWND_TOPMOST,
                 rect.x(), rect.y(), rect.width(), rect.height(), flags);
}

void requestClose(const QList<quint32>& pids)
{
    forEachAppWindow(pids, [](HWND hwnd) { PostMessageW(hwnd, WM_CLOSE, 0, 0); });
}

quint64 tickCountMs()
{
    return GetTickCount64();
}

quint32 bootId()
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management\\PrefetchParameters",
        L"BootId", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return status == ERROR_SUCCESS ? value : 0;
}

QString fileDescription(const QString& exePath)
{
    const std::wstring path = exePath.toStdWString();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0)
        return {};

    QByteArray data(int(size), Qt::Uninitialized);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return {};

    struct LangCodePage { WORD lang; WORD codePage; }* translation = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translation), &len)
        || len < sizeof(LangCodePage))
        return {};

    const QString key = QString("\\StringFileInfo\\%1%2\\FileDescription")
                            .arg(translation->lang, 4, 16, QChar(u'0'))
                            .arg(translation->codePage, 4, 16, QChar(u'0'));
    wchar_t* value = nullptr;
    if (!VerQueryValueW(data.data(), reinterpret_cast<LPCWSTR>(key.utf16()),
                        reinterpret_cast<void**>(&value), &len)
        || len == 0)
        return {};
    return QString::fromWCharArray(value).trimmed();
}

} // namespace winutil
