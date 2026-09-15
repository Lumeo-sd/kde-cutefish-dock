# Крок 2: Terminal — Qt6/KF6 Wayland port (Plasma 6)

Гілка: `fork-qt6-wayland` (крок 1 — `5d464d3`, `ee6654d`; крок 2 — див. коміти нижче)
Framework: локальний `cutefish-framework` (`appearance`, селективна збірка) — reuse, без змін
FishUI: локальний Qt6-форк (`~/.local/lib64/qt6/qml/FishUI`) — runtime-only, без змін
Upstream base: `56cf752` (Qt5, X11-нейтральний; репозиторій НЕ заархівовано — можливий майбутній PR)
Система: Fedora 44, Plasma 6.7.5, Qt 6.11.2, real Wayland (`XDG_SESSION_TYPE=wayland`)

## Що змінилось («було → стало»)

| Область | Upstream Qt5 | Порт Qt6 |
|---|---|---|
| CMake (корінь) | `cmake_min 3.5`, C++11, `find_package(Qt5)`, `Qt5::`, `qt5_create_translation`, `INSTALL_QMLDIR` через qmake, `install(... /usr/bin, /usr/share/...)` | `cmake_min 3.16`, C++17, `find_package(Qt6 6.5)`, `Qt6::`, `qt6_add_translation`, `INSTALL_QMLDIR` через `qtpaths6` з `-D`-override, `GNUInstallDirs` (`${CMAKE_INSTALL_BINDIR}`, `${CMAKE_INSTALL_DATADIR}`) |
| CMake (qmltermwidget) | `Qt5::Core/Quick/Gui/Widgets` | `Qt6::` + `Qt6::Core5Compat` (там живуть `QTextCodec`/`QRegExp`) |
| QML-імпорти | `QtGraphicalEffects 1.0` | `Qt5Compat.GraphicalEffects 1.0` (`src/qml/main.qml`, `ImageButton.qml`) |
| Запуск встановленого бінарника | — (не ставився) | `engine.addImportPath(applicationDirPath()+"/../lib64/qt6/qml")` — Qt6 ігнорує `QT_QML_IMPORT_PATH`, без цього `FishUI`/`Cutefish.TermWidget` не резолвляться з `.desktop`-запуску |
| Переклади | хардкод `/usr/share/cutefish-terminal/translations` | `QStandardPaths::locateAll(GenericDataLocation, "cutefish-terminal/translations")` — патерн launcher |
| `.desktop` | `Exec=/usr/bin/cutefish-terminal` | `Exec=cutefish-terminal` (PATH; `~/.local/bin` у PATH) → іконка в сітці додатків |
| Шрифт | `Cutefish-Light` Foreground `0,0,0`, `Cutefish-Dark` Foreground `24,240,24` (зелений) | обидві схеми Foreground/ForegroundIntense `255,255,255`, `Cutefish-Light` ForegroundFaint `200,200,200` (фон прозорий, `backgroundOpacity: 0` — читається на темному вікні) |
| Назва таба | `QDir("").dirName()` = `"."` поки не готовий foreground-process info | `KSession::getTitle()` — fallback-ланцюжок: currentDir → initialWorkingDirectory (крім літералів `$PWD`/`$HOME`) → foregroundProcessName → `tr("Terminal")`; крапки більше немає |

### Qt6-правки вендореного `qmltermwidget` (по файлах)

- `lib/Session.cpp`: `QString::sprintf` видалено в Qt6 → `QString::asprintf` (3 місця, `done()`); `+ #include <QRegularExpression>` (використовується в `setUserTitle`, був лише forward-declare); коментар Qt6-ери.
- `lib/Vt102Emulation.cpp`: `- #include <QByteRef>` (клас видалено в Qt6, include не використовувався); `newValue[j] = QChar(tokenBuffer[…])`, `coords[n] = QChar(…)` — неявні `wchar_t`/`int → QChar` заборонені.
- `lib/TerminalDisplay.cpp` / `.h`: `geometryChanged` → `geometryChange … override` (перейменовано в `QQuickItem` Qt6); `simulateWheel` — новий конструктор `QWheelEvent(pos, globalPos, pixelDelta, angleDelta, buttons, modifiers, phase, inverted)` (старий видалено).
- `src/ksession.cpp`: `getenv("SHELL")` — `QString != NULL` не компілюється → `fromLocal8Bit` + `QStringLiteral("/bin/bash")`; `getTitle()` — fallback-ланцюжок (див. таблицю).
- `lib/Pty.cpp/.h`, `lib/kpty*.cpp/.h`, `lib/kprocess.h`, `lib/Filter.cpp`, `lib/ColorScheme.cpp`, `lib/HistorySearch.*`, `lib/KeyboardTranslator.cpp`, `lib/TerminalDisplay.*` — дрібні Qt6-адаптації (headers, `QTextCodec`/`QRegExp` через Core5Compat, deprecations).

Wayland-специфіки НЕМАЄ (аудит 0.2): звичайний toplevel (`FishUI.Window`), LayerShellQt не потрібен.

## Збірка / встановлення / запуск (private prefix)

```
cmake -S terminal -B /tmp/opencode/build-terminal \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local \
  -DINSTALL_QMLDIR=$HOME/.local/lib64/qt6/qml
cmake --build /tmp/opencode/build-terminal -j$(nproc)
cmake --install /tmp/opencode/build-terminal
update-desktop-database ~/.local/share/applications/; kbuildsycoca6
```

Результат: `~/.local/bin/cutefish-terminal`,
`~/.local/share/applications/cutefish-terminal.desktop`,
`~/.local/lib64/qt6/qml/Cutefish/TermWidget/` (plugin `.so`, `qmldir`,
`QMLTermScrollbar.qml`, `color-schemes/`, `kb-layouts/`),
переклади в `~/.local/share/cutefish-terminal/translations/`.
Запуск — з сітки додатків або `~/.local/bin/cutefish-terminal`, БЕЗ env-костилів.

## Перевірено на Plasma 6.7.5 (Fedora 44, Wayland)

- Збірка чиста (попередження-deprecations; фаталів немає), інсталяція в `~/.local`.
- Запуск встановленого бінарника без `QML2_IMPORT_PATH` — QML грузиться, сесія стартує, шел працює.
- Іконка Terminal у сітці додатків (після `kbuildsycoca6`).
- Білий шрифт в обох схемах (скрін + живе підтвердження юзера).
- Таб: `"."` → `"Terminal"` (fallback) → ім'я каталогу після готовності process info (скрін: `Terminal`, `cutefish doc original`; KWin caption повний).
- Розгортання □: KWin ground truth — `geom=0,25 1920x992` = `maximizeArea`, док (`380,1017 1160x58`) і статусбар (`0,0 1920x25`) не перекриваються. F11 — справжній fullscreen поверх панелей (штатно).
- Кнопки шапки `− □ ×` на місці (скрін-кроп), `showMinimized()` — `xdg-toplevel.set_minimized`, кодовий шлях тривіальний.

## Ключові знахідки

1. **`QDir("").dirName()` повертає `"."`** (перевірено окремим тестом на Qt6) — ось і «крапка» в табі: `currentDir()` порожній, доки не валідний foreground-process info (`/proc`), що на старті займає секунди. Лікується fallback-ланцюжком, а не очікуванням сигналу.
2. **Схеми беруться з import-path**, не з qrc: плагін у `initializeEngine` ставить `COLORSCHEMES_DIR=$importpath/Cutefish/TermWidget/color-schemes`. Без розгорнутого каталога схем плагін мовчки падає на вбудований дефолт — тому перший запуск показав чорний шрифт.
3. **Qt6 ігнорує `QT_QML_IMPORT_PATH`** — єдиний робочий шлях для `.desktop`-запуску: `addImportPath` у коді (патерн dock).
4. **У top-level `CMakeLists` не було `INSTALL_QMLDIR`-override** (безумовний `execute_process` — `-D` ігнорувався) і всі `install()` — в абсолютний `/usr` (потрібен root). Тому термінал ніколи не ставився — патерн виправлення скопійовано з launcher (`GNUInstallDirs`).
5. **Відкритий таб — літерали `$PWD`/`$HOME`** (`openTab("$PWD")` без розгортання) — для заголовка нейтралізовано фільтром у `getTitle()`; сам виклик не чіпали (поза обсягом).

## Що лишилось

- Варнінги (не фатали): `Qt.labs.settings` deprecated (→ `QtCore` Settings), injection `event`/`mouse`/`close`-параметрів у QML-хендлери, `No such slot TerminalDisplay::close()`, `Created graphical object was not placed in the graphics scene` (`Terminal.qml:29`).
- `openTab("$PWD"/"$HOME")` — розгортати змінні оточення при створенні таба.
- `Qt::AA_EnableHighDpiScaling` у `main.cpp` — deprecated в Qt6 (як у statusbar).
- In-tree `terminal/build/` — застарілий кеш (збірка ведеться в `/tmp/opencode/build-terminal`); прибрати або ігнорувати.
- Юзер-верифікація кліком у GUI (фінальний чекпоінт); за словами юзера — «варіант ще потребує правок але він робочий, залишимо».
