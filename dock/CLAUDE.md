@../CLAUDE.md

# CutefishOS Dock — Qt6/KF6 порт

## Збірка/запуск (деталі — у CLAUDE.md верхнього рівня)

```
cmake -S dock -B /tmp/opencode/build-dock-dbg -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/build-dock-dbg -j$(nproc)
pkill -x cutefish-dock; sleep 1          # потім копіювати ОКРЕМИМ рядком:
cp /tmp/opencode/build-dock-dbg/cutefish-dock ~/.local/bin/cutefish-dock
QT_FORCE_STDERR_LOGGING=1 setsid ~/.local/bin/cutefish-dock >/tmp/opencode/dock.log 2>&1 &
```

## Ключові файли

- `src/mainwindow.cpp/.h` — вікно, LayerShellQt, `addDesktopFile` (drag&drop),
  двофазний show/hide (`m_shrinkTimer`, `stripRect()`).
- `src/xwindowinterface.cpp/.h` — KWayland `org_kde_plasma_window_management`
  (active/minimize/desktopFilePath), `KeyboardInteractivityNone` у footer.
- `src/applicationmodel.cpp` — модель (launcher+pinned+closed+trash), pin/unpin,
  збереження в `dock_pinned.conf`.
- `src/systemappmonitor.cpp` — сканування XDG-каталогів (user-first, дедуп).
- `src/utils.cpp` — `desktopPathFromMetadata` (appId-first, потім pid),
  `readInfoFromDesktop`.
- `qml/main.qml` — кореневий `DropArea` (зовнішні `.desktop`-дропи),
  opacity за `dockHidden`.
- `qml/AppItem.qml` — Pin/Unpin меню, `visible: model.desktopFile !== ""`.
- `cutefish-dock.desktop` — `X-KDE-Wayland-Interfaces` (KWin permission gate).

## Локальні файли конфігів

`~/.config/cutefishos/{dock,dock_pinned,appearance}.conf` — ручне редагування
дозволяється, dock перечитує на старті.

## Обмеження

- WIds — власні послідовні id, не плазмові; `map<quint64, PlasmaWindow*>`.
- Авто-хайд = стрип ~2px + QML opacity (не unmapping); FakeWindow вимкнено.
- D-Bus: `com.cutefish.Dock` / `/Dock` (методи `add`, `remove`, `pinned`).