# CutefishOS → Qt6/KF6 Wayland — Workspace

## Контекст

Порт оригінального CutefishOS dock (Qt5/KF5 → Qt6/KF6) під Plasma 6 Wayland.
Три форкнутих репо (апстрім архівовано 2026-08-30, апстріму нема):

| Каталог | Вихідне репо | Роль |
|---|---|---|
| `dock/` | cutefishos/dock | сам dock (C++/QML) |
| `fishui/` | cutefishos/fishui | QML-фреймворк (теми, вікна, popupтултипи) |
| `cutefish-framework/` | cutefishos/libcutefish | бібліотека, тут постачає `appearance` (daemon + конфіг) |

Усі три на гілці `fork-qt6-wayland` (локально, НЕ запушено).

Головне правило: **максимально зберігати оригінальний C++/QML**; міняти лише
X11-only частини (NET::Dock/struts → LayerShellQt, трекінг вікон →
org_kde_plasma_window_management). Вигляд/анімації оригіналу не переписувати.

## Середовище

- Plasma 6.7.5 Wayland, Fedora 44, kwin-6.7.5. Приватний prefix `~/.local`.
- Запуск dock (env для логів; магія ліб — через INSTALL_RPATH, див. нижче):
  ```
  QT_FORCE_STDERR_LOGGING=1 ~/.local/bin/cutefish-dock
  ```
- Автозапуск: `~/.config/autostart/cutefish-dock.desktop`
  (`Exec=/home/mutagen/.local/bin/cutefish-dock`, стартує після plasma-desktop).
- Конфіги (ручне редагування):
  - `~/.config/cutefishos/dock.conf` — позиція/стиль/visibility
  - `~/.config/cutefishos/dock_pinned.conf` — закріплені додатки
  - `~/.config/cutefishos/appearance.conf` — `[General] darkMode=true`
    (fallback, коли daemon недоступний)
- D-Bus: сервіс `com.cutefish.Dock`, об'єкт `/Dock`
  (busctl get-property: `visibility`,
  `primaryGeometry 0 0 464 58`, методи `add`/`remove`/`pinned`).

## Збірка

Три окремі build-дерева в `/tmp/opencode/` (періодично чисті, збірка перевірена):

```
cmake -S dock -B /tmp/opencode/build-dock-dbg \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local
cmake --build /tmp/opencode/build-dock-dbg -j$(nproc)
cp /tmp/opencode/build-dock-dbg/cutefish-dock ~/.local/bin/

cmake -S fishui -B /tmp/opencode/build-fishui \
  -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/build-fishui -j$(nproc)
cmake --install /tmp/opencode/build-fishui
# appearance.so ставиться fishui-інсталером не завжди — синхронізувати вручну:
cp /tmp/opencode/build-fishui/cutefish-framework-appearance-build/*.so ~/.local/lib64/
```

**Важливо про ліби:** `cmake --install` fishui НЕ ставить appearance-лібу в
`~/.local/lib64` — копіювати вручну (вище). `libFishUI.so` має
`INSTALL_RPATH=$ORIGIN/../../..` (тобто `<prefix>/lib64`), тому dock
запускається БЕЗ `LD_LIBRARY_PATH` (це критично для автозапуску). Qt6 не читає
`QT_QML_IMPORT_PATH`; dock додає import path сам
(`applicationDirPath()+"/../lib64/qt6/qml"`).

Перезапуск dock після збірки:
`pkill -x cutefish-dock; sleep 1` — потім копіювати бінарник ОКРЕМО (не `&&`,
інакше "Text file busy"), потім `setsid ...dock > log 2>&1 &`.

## Ключові рішення (хронологія)

- 2026-09-12: dock без авто-хайду (default visibility=0/AlwaysShow), темна тема
  (`darkMode=true`), двофазний show/hide (fade 200ms → resize 260ms).
- 2026-09-12: `KeyboardInteractivityNone` замість OnDemand — інакше dock стає
  активним вікном KWin і мінімізація кліком не працювала.
- 2026-09-12: drag&drop додатків у dock — кореневий `DropArea` в main.qml
  приймає `.desktop` URL і кличе `mainWindow.addDesktopFile()` (Q_INVOKABLE).
- 2026-09-12: `SystemAppMonitor` сканує весь XDG-шлях (~/.local/share/applications,
  XDG_DATA_DIRS, flatpak-експорти; пріоритет user-first + дедуп). `Utils::
  desktopPathFromMetadata` спершу матчить за Wayland appId (baseName,
  StartupWMClass, Icon), потім pid/cmdline — виправляє "не всі додатки можна
  припінити" (Pin-меню ховається, коли `desktopFile==""`).
- 2026-09-12: автостарт через `~/.config/autostart/`; rpath на FishUI QML
  (`$ORIGIN/../../..`).
- Раніше: LayerShellQt (layer top, плаваюча центрована панель, exclusive zone)
  замість NET::Dock/struts; трекінг вікон через KWayland
  `org_kde_plasma_window_management` (map<quint64, PlasmaWindow*>);
  авто-хайд = "стрип" (~2px) + QML opacity, не unmapping; WIds — власні.

## Обмеження/нюанси

- KWin 6 scripting: `workspace.windows` не існує — `workspace.windowList()`;
  `setInterval` не визначено; `console.log` іде в прихований канал —
  використовувати `console.error` (`journalctl  _PID=$(pgrep -x kwin_wayland)`).
- KWin permission gate: desktop-файл додатка має мати `X-KDE-Wayland-Interfaces`
  (`dock/cutefish-dock.desktop` → `org_kde_plasma_window_management`);
  зіставлення по canonical Exec через KApplicationTrader.
- KWayland: `ConnectionThread::fromApplication()` + `Registry::setup()`,
  без event queue.
- Сенсорних/мишкових перевірок перетягування немає під цю сесію —
  юзер тестує вручну (yudotool/xdotool/wtype відсутні).

## Дивись також

- `PROGRESS.md` — журнал стану (змінюється щокроку)
- Звіт кроку 2: `dock/STEP2-DOCK-WAYLAND.md`, аудит: `STEP0-AUDIT.md` (у dock/)
- `AGENTS.md` — імпортує цей файл (OpenCode читає AGENTS.md нативно)