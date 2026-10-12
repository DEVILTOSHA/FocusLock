# FocusLock (Qt 6 / C++20)

Give each app a session time budget. When the budget runs out, FocusLock covers the
app's window with an always-on-top lock screen that stays until the app exits.

## Build

Needs Qt 6.8+ (Widgets), CMake 3.21+, and MinGW or MSVC. With the Qt online installer
defaults on this machine:

```sh
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
cmake --build build
./build/FocusLock.exe
```

After each build, `windeployqt` copies the Qt DLLs into `build/`, so the exe runs
on its own. Turn that off with `-DFOCUSLOCK_DEPLOY_ON_BUILD=OFF`.

You can also open `CMakeLists.txt` in Qt Creator.

## How it works

| File | Role |
|---|---|
| `src/ProcessMonitor.*` | Worker-thread engine. Each second it calls `CreateToolhelp32Snapshot`, counts down and persists budgets, and emits `timeUp` / `unlocked` |
| `src/TrustedClock.*` | Time line for cooldowns that the system clock can't change (tick counter plus network time sync) |
| `src/WinUtil.*` | Win32 helpers: process snapshot, `EnumWindows` main-window lookup, `GetWindowRect` (refined with DWM frame bounds), topmost placement, `WM_CLOSE`, exe description |
| `src/OverlayDialog.*` | Frameless `WindowStaysOnTopHint` lock screen that follows the target window every 200 ms |
| `src/MainWindow.*`, `src/WatchCard.*` | Watchlist UI, tray icon, settings (`QSettings`) |
| `src/Theme.h` | Dark palette, stylesheet, and the programmatic app icon |

Budget rules:
- The budget only counts down while the app has a visible top-level window.
- It refills **24 hours after it first starts counting down** (`kRefillCooldownSecs` in
  `src/WatchTypes.h`). Closing the app doesn't refill it, so reopening a spent app locks it again immediately.
- While that 24 h window runs, the app's limit is frozen. You can't raise or lower it, or remove
  the app, until the budget refills.
- Usage is saved under `HKCU\Software\FocusLock\FocusLock\usage`, so restarting FocusLock or
  removing and re-adding an app doesn't reset it.
- The cooldown never reads the system clock. `src/TrustedClock.*` keeps its own time line:
  `GetTickCount64` within a boot (it counts sleep and ignores clock changes), and across reboots
  it credits the downtime using the `Date` header of HTTPS servers (Google, Cloudflare, Microsoft).
  If it can't reach the network, no downtime is credited, so the cooldown can run slow but never fast.
- Esc and Alt+F4 can't dismiss the overlay. It lifts when the process exits, its last window
  closes, or the budget refills.
- **Close <app>** sends `WM_CLOSE`, so the app can still ask to save. The overlay shrinks to a
  card at the top edge for 15 s so that prompt stays reachable.
- Closing the main window sends it to the tray, and timers keep running. Quit from the tray menu.
