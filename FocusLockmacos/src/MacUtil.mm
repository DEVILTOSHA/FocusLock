#include "MacUtil.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <libproc.h>
#include <sys/sysctl.h>
#include <time.h>

#include <vector>

namespace macutil {

namespace {

// Smaller windows are toolbars, tooltips and other helpers, not the app itself.
constexpr int kMinWindowSide = 100;

QRect toQRect(const CGRect& r)
{
    return QRect(int(r.origin.x), int(r.origin.y), int(r.size.width), int(r.size.height));
}

} // namespace

QHash<QString, QList<quint32>> snapshotProcesses()
{
    QHash<QString, QList<quint32>> result;

    const int bytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (bytes <= 0)
        return result;
    // Room for processes started between the two calls.
    std::vector<pid_t> pids(size_t(bytes) / sizeof(pid_t) + 64);
    const int filled = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), int(pids.size() * sizeof(pid_t)));
    if (filled <= 0)
        return result;

    char path[PROC_PIDPATHINFO_MAXSIZE];
    const size_t count = size_t(filled) / sizeof(pid_t);
    for (size_t i = 0; i < count; ++i) {
        if (pids[i] <= 0 || proc_pidpath(pids[i], path, sizeof(path)) <= 0)
            continue; // gone, or another user's process
        const QString full = QString::fromUtf8(path);
        result[full.mid(full.lastIndexOf(u'/') + 1).toLower()].append(quint32(pids[i]));
    }
    return result;
}

std::optional<TargetWindow> findMainWindow(const QList<quint32>& pids)
{
    if (pids.isEmpty())
        return std::nullopt;

    @autoreleasepool {
        // Every window, ordered front to back, including minimized and hidden ones.
        NSArray* windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
            kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID));

        std::optional<TargetWindow> offscreen;
        for (NSDictionary* info in windows) {
            const quint32 owner = [info[(__bridge id)kCGWindowOwnerPID] unsignedIntValue];
            if (!pids.contains(owner))
                continue;
            if ([info[(__bridge id)kCGWindowLayer] intValue] != 0)
                continue; // menus, status items, panels above normal windows
            if ([info[(__bridge id)kCGWindowAlpha] doubleValue] <= 0.0)
                continue;

            CGRect bounds = CGRectZero;
            CGRectMakeWithDictionaryRepresentation(
                (__bridge CFDictionaryRef)info[(__bridge id)kCGWindowBounds], &bounds);
            if (bounds.size.width < kMinWindowSide || bounds.size.height < kMinWindowSide)
                continue;

            TargetWindow target;
            target.handle = [info[(__bridge id)kCGWindowNumber] unsignedIntValue];
            if ([info[(__bridge id)kCGWindowIsOnscreen] boolValue]) {
                // The list is in z-order, so the first visible window is the app's front one.
                target.rect = toQRect(bounds);
                return target;
            }
            if (!offscreen) {
                target.minimized = true;
                offscreen = target;
            }
        }
        return offscreen;
    }
}

quint32 frontmostPid()
{
    @autoreleasepool {
        NSRunningApplication* app = NSWorkspace.sharedWorkspace.frontmostApplication;
        return app ? quint32(app.processIdentifier) : 0;
    }
}

void makeOverlayWindow(quintptr widgetWinId)
{
    @autoreleasepool {
        NSView* view = (__bridge NSView*)reinterpret_cast<void*>(widgetWinId);
        NSWindow* window = view.window;
        if (!window)
            return;
        // Above normal and floating windows, and present on full-screen Spaces too.
        window.level = NSStatusWindowLevel;
        window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                                  | NSWindowCollectionBehaviorFullScreenAuxiliary
                                  | NSWindowCollectionBehaviorStationary
                                  | NSWindowCollectionBehaviorIgnoresCycle;
        // Qt::Tool windows are NSPanels, which hide whenever FocusLock isn't the active app.
        window.hidesOnDeactivate = NO;
    }
}

void activateSelf()
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // The macOS 14 replacement, -activate, is only a request the active app can refuse.
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
}

void requestClose(const QList<quint32>& pids)
{
    @autoreleasepool {
        for (quint32 pid : pids)
            [[NSRunningApplication runningApplicationWithProcessIdentifier:pid_t(pid)] terminate];
    }
}

quint64 tickCountMs()
{
    // CLOCK_MONOTONIC_RAW keeps counting while the Mac sleeps.
    return clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) / 1000000;
}

quint32 bootId()
{
    char uuid[64] = {};
    size_t len = sizeof(uuid) - 1;
    if (sysctlbyname("kern.bootsessionuuid", uuid, &len, nullptr, 0) != 0 || len == 0)
        return 0;
    const quint32 id = quint32(qHash(QByteArray(uuid)));
    return id ? id : 1;
}

QString bundleExecutable(const QString& bundlePath)
{
    @autoreleasepool {
        NSBundle* bundle = [NSBundle bundleWithPath:bundlePath.toNSString()];
        if (NSString* exe = bundle.executablePath)
            return QString::fromNSString(exe.lastPathComponent);
        if (NSString* name = [bundle objectForInfoDictionaryKey:@"CFBundleExecutable"])
            return QString::fromNSString(name);
        return {};
    }
}

QString bundleDisplayName(const QString& bundlePath)
{
    @autoreleasepool {
        NSBundle* bundle = [NSBundle bundleWithPath:bundlePath.toNSString()];
        for (NSString* key in @[@"CFBundleDisplayName", @"CFBundleName"]) {
            id value = [bundle objectForInfoDictionaryKey:key];
            if ([value isKindOfClass:NSString.class] && [value length] > 0)
                return QString::fromNSString(value).trimmed();
        }
        return {};
    }
}

} // namespace macutil
