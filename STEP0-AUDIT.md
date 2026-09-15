# Крок 0 — Аудит: порт CutefishOS terminal на Qt6/KF6 + Plasma 6 Wayland

Дата: 2026-09-14
Система: Fedora 44 (KDE Plasma), Plasma 6, Qt 6, KF6, сесія — real Wayland
(`XDG_SESSION_TYPE=wayland`), KWin 6.
Джерело: https://github.com/cutefishos/terminal (README без згадки про X11; репозиторій
**НЕ заархівовано upstream** — форк дає шлях до майбутнього PR).
Локальний форк: `terminal/`, гілка `fork-qt6-wayland` (планується; зараз HEAD=`56cf752`
original/main, remote = upstream clone).

---

## 0.0 Рішення з аудиту (коротко)

| # | Питання аудиту | Відповідь | Наслідок |
|---|---|---|---|
| 0.1 | Стан вбудованого `qmltermwidget` | **Вендорена повна копія** (Konsole-похідне `lib/`: `kpty*`, `kprocess*`, `Vt102Emulation`, `Emulation`, `Screen`, `Session` + `src/` плагін `qmltermwidget_plugin`). **НЕ submodule** (`.gitmodules` немає), має власний `CMakeLists.txt` (`add_definitions(HAVE_POSIX_OPENPT ...)`, Qt5-епоха) | **Найбільший шматок порту**: Qt5→Qt6 у вендореній копії (C++11→C++17, Qt6 headers, `kpty` API) |
| 0.2 | X11/Shell-специфіка в `src/` | `terminal/src/main.cpp` — **тільки** `QApplication`/`QGuiApplication`; grep `QX11Info|Xcb|Display*|X11|wayland` по `src/` → **0 shell-коду** | **Крок LayerShellQt пропускається повністю** (звичайне top-level вікно, порт без Wayland-специфіки) |
| 0.3 | Залежність від fishui | **Runtime-only**: QML-імпорти `import FishUI 1.0` у `main.qml`/`Terminal.qml`/`SettingsDialog.qml`/`ImageButton.qml` (`FishUI.Window`, `Theme`, `Units`, `DesktopMenu`, `WindowBlur`, `TabBar`, `TabView`). **C++ НЕ лінкується** проти fishui | **Переюзаємо локальний Qt6 fishui** (та сама стратегія, що в dock/statusbar) — без додаткового порту |

### Додаткові факти (детерміновано)
- `CMakeLists.txt`: `cmake_minimum_required(3.5)`, `CMAKE_CXX_STANDARD 11`,
  `find_package(Qt5 REQUIRED Core Gui Quick QuickControls2 Widgets DBus LinguistTools)`,
  `add_subdirectory(qmltermwidget)`, решта — переклади + install у `/usr/bin`.
  **Qt5-специфіка** — це саме те, що замінюється на Qt6 (`Qt5::` → `Qt6::`, `ping5_x` →
  `ping6_x`, transl-макроси).
- Процес-хелпер: `src/processhelper.{h,cpp}` через `ProcessHelper` у QML-контексті
  (`main.cpp` — setContextProperty). Без shell-залежностей.
- `qmltermwidget/src/qmltermwidget_plugin.cpp` — `Q_PLUGIN_METADATA` + qmldir для
  `QMLTermWidget`; ядро (`lib/`) — Konsole (Blackpill): `kpty`, `Screen`, `Vt102Emulation`,
  `History`, `TerminalDisplay` — це і є реальний обсяг порту Qt6.

---

## 0.1 Вендорений `qmltermwidget`: версія та стратегія

**Стан**: повна копія в репо (`terminal/qmltermwidget/`), ~300+ файлів, CMake-збірка як
складова `cutefish-terminal`. Дивимось на **Qt5-залежності в коді**:

- `lib/kpty.cpp`, `lib/kptydevice.cpp`, `lib/kptyprocess.cpp` — Konsole-порт (kpty),
  утилізує `pty.h`/`openpty` (POSIX), **не** X11/wayland.
- `lib/Vt102Emulation.cpp`, `lib/Emulation.cpp`, `lib/Screen.cpp` — чистий Qt (QChar/QColor).
- `src/qmltermwidget_plugin.cpp` — `Q_PLUGIN_METADATA(IID)` + `qt_register_resource`.

**Рішення (крок 1)**: **ручний Qt6-порт вендореної копії** (БЕЗ bump до upstream
`Swordfish90/qmltermwidget` — оскільки Cutefish-версія має власну інтеграцію з
`ksession`/`kpty`, і upstream-копія може мати інші патчі; bump — лише якщо виявиться
непортовним шматок). Ключові Qt6-аспекти, що перевіряються в кроку 1:
- `QTextCodec` → Qt6 видалив `QTextCodec::setCodecForLocale` (перевірити `ShellCommand`),
- `QProcess::startDetached` сигнатура (Qt6 — `qint64 *pid`),
- `Qt::WellKnownCPUs`/`features.h`, `Q_DECL_METATYPE`,
- `Q_OBJECT`/moc у Qt6.8 (нові правила `QObject::tr`/namespaced).

---

## 0.2 Wayland / LayerShell — **ПРОПУЩЕНО** ✅

`grep 'QX11Info|Xcb|Display \*|X11|wayland|xcb' terminal/src/*.cpp` → **0 збігів**.
Це звичайний top-level (`QQmlApplicationEngine` + `FishUI.Window`), без fullscreen-overlay
і без X11-specific позиціювання. Тому:
- **НЕ** додаємо LayerShellQt,
- **НЕ** робимо жодних Wayland-адаптацій у C++,
- весь порт — це **лише Qt5→Qt6 (qmltermwidget + обгортка)** плюс fishui-ритм у QML.

---

## 0.3 Залежність від fishui — **Runtime-only** ✅

```
qml/main.qml:      import FishUI 1.0 as FishUI → FishUI.Window / Theme / Units / WindowBlur / TabBar / TabView / DesktopMenu
src/qml/Terminal.qml: FishUI.Theme / Units
src/qml/SettingsDialog.qml: FishUI.Window / Theme / Units
src/qml/ImageButton.qml: FishUI.Theme
```

С++ не викликає fishui-класів (немає `#include <FishUI/...>`). Отже в **кроку 1**:
- QML-залежність задовольняє локальний **Qt6 fishui** (встановлений як QML-модуль),
- у CMakeLists додається лише runtime-вимога (не лінк).

---

## Підсумок аудиту (для кроку 1)

**Обсяг роботи**: середній. Ядро — **порт вендореного `qmltermwidget` (Konsole lib + плагін)
з Qt5 на Qt6** + заміна `Qt5::`* на `Qt6::`* у CMake + C++11→C++17 + дрібні Qt6 API-правки
(`QTextCodec`, `QProcess::startDetached`, moc). **Жодного** Wayland/X11-шару не потрібно.

**Реюз локальних форків**: fishui (runtime), щоб уникнути drag-n-drop/іконтemm-регресу.
`qmltermwidget` — **портуємо вендорену копію** (bump upstream не потрібен).

Формат звіту кроку 1: той самий, що в dock/statusbar — окремі коміти на кожен логічний
шматок (CMake→Qt6, залежності qmltermwidget, FishUI-QML, збірка/інсталяція).
