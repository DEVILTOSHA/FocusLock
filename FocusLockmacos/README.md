# FocusLock for macOS (Qt 6 / C++20)

The macOS port of FocusLock. Give each app a session time budget. When the budget runs
out, FocusLock covers the app's window with a lock screen that stays until the app quits.

> **Status:** written on Windows and not yet compiled or run on a Mac. Expect to fix a
> few build errors on the first build, and to run through the test list below.

## Build

Needs macOS 12+, Xcode (or the Command Line Tools), CMake 3.21+, and Qt 6.8+ for macOS
from the Qt online installer.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/Qt/6.12.0/macos
cmake --build build
open build/FocusLock.app
```

Without Ninja, drop `-G Ninja`, or use `-G Xcode` to get an Xcode project. You can also
open `CMakeLists.txt` in Qt Creator.

To share the app with Macs that don't have Qt, bundle the frameworks and make a disk image:

```sh
~/Qt/6.12.0/macos/bin/macdeployqt build/FocusLock.app -dmg
```

Or configure with `-DFOCUSLOCK_DEPLOY_ON_BUILD=ON` to run `macdeployqt` after every build.
Unsigned apps from the internet are blocked by Gatekeeper. Other people need to
right-click → Open the first time, or you can sign and notarize the app with an Apple
Developer account.

## What changed from the Windows version

Only the parts that talked to Windows directly. The budget engine, trusted clock, and UI
are the same code.

| Windows | macOS |
|---|---|
| `WinUtil.*` | `MacUtil.h` / `MacUtil.mm` (Objective-C++) |
| `CreateToolhelp32Snapshot` | `proc_listpids` + `proc_pidpath` |
| `EnumWindows` + `GetWindowRect` | `CGWindowListCopyWindowInfo` (front-to-back z-order) |
| `HWND_TOPMOST` | `NSStatusWindowLevel`, joins every Space including full-screen ones |
| `WM_CLOSE` | `NSRunningApplication terminate` (same as choosing Quit) |
| `GetTickCount64` | `clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)`, which counts sleep |
| Registry `BootId` | `kern.bootsessionuuid` |
| `.exe` picker + version resource name | `.app` picker + `Info.plist` name and executable |
| Registry settings | `~/Library/Preferences/com.focuslock.FocusLock.plist` |
| Tray icon | Menu bar icon |

Other macOS-specific behaviour:

- **Matching:** apps are matched by the executable inside the bundle
  (`Contents/MacOS/<name>`), not the bundle name.
- **Windows that count:** minimized windows, hidden apps (Cmd+H), and windows on another
  Space still count as open and keep the budget draining, the same as minimized windows
  on Windows.
- **Quitting a locked app:** while a locked app is quitting, the lock card moves to the
  bottom edge of its window. macOS save prompts drop down from the top as sheets, so
  they stay clickable.
- **Theme:** font sizes are scaled by 4/3, because macOS draws 1 pt as 1 px where Windows
  draws it larger.
- **Permissions:** none are required. Window positions and owners are readable without
  Screen Recording permission. Only window titles would need it, and FocusLock doesn't
  read them.

## Test checklist (on a Mac)

1. Add TextEdit with a 1-minute budget, open it, and wait. The lock screen should cover
   its window.
2. Cmd+Tab to TextEdit. FocusLock should take focus back within 200 ms.
3. Make TextEdit full screen. The lock screen should appear on its Space.
4. Type something unsaved, then click **Quit TextEdit**. The save sheet should stay
   reachable above the card.
5. Minimize TextEdit while it's locked. A centred card should remain.
6. Restart FocusLock. The budget and "Refills in" time should carry over.

## Known gaps

- **Quitting FocusLock:** you can still quit FocusLock from the menu bar icon, the same
  as on Windows. The macOS fix would be a `launchd` daemon that enforces the locks.
- **Apps with no windows:** some apps keep hidden windows after you close them all. If
  one keeps draining with no windows open, `findMainWindow` in `MacUtil.mm` needs a
  stricter filter for it.
