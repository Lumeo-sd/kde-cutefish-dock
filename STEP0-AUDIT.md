# Крок 0 — Аудит: порт CutefishOS settings на Qt6/KF6 + Plasma 6 Wayland

Дата: 2026-09-14
Система: Fedora 44 (KDE Plasma), Plasma 6, Qt 6, KF6, сесія — real Wayland
(`XDG_SESSION_TYPE=wayland`), KWin 6.
Джерело: https://github.com/cutefishos/settings (upstream НЕ заархівовано —
форк дає шлях до майбутнього PR, як у terminal).
Локальний форк: `settings/`, гілка `fork-qt6-wayland` (HEAD=`de04322` upstream main).

---

## 0.0 Рішення з аудиту (коротко)

| # | Питання аудиту | Відповідь | Наслідок |
|---|---|---|---|
| 0.1 | Стан upstream щодо Qt6 | **Вже Qt6**: `find_package(Qt6)`, `qt6_add_translation`, `qt6_add_dbus_adaptor`, C++17, QML на `Qt5Compat.GraphicalEffects`. Qt5-специфіки немає | **CMake-порту Qt5→Qt6 не потрібно** — лише private-prefix (патерн terminal/launcher) |
| 0.2 | X11/Shell-специфіка в `src/` | `grep QX11\|KX11\|xcb\|NET::\|KWindowSystem\|LayerShell` по `src/` + CMake → **0 збігів**. Звичайний toplevel (`FishUI.Window` в `qml/main.qml`), без struts, без fake-вікон | **Крок LayerShellQt пропускається повністю** (як у terminal) |
| 0.3 | Залежність від fishui | **Runtime-only**: ~20 QML-файлів `import FishUI 1.0` (`Window`, `Theme`, `WindowBlur`, ...). **C++ НЕ лінкується** проти fishui (0 `#include <FishUI`) | **Переюзаємо локальний Qt6 fishui** (`~/.local/lib64/qt6/qml/FishUI`) — без додаткового порту |

### Додаткові факти (детерміновано)

- `src/application.cpp` реєструє власний QML-модуль `Cutefish.Settings 1.0`
  через `qmlRegisterType` (DockSettings, FontsModel, Brightness, Battery, ...,
  `Password`-singleton) + `registerApplicationsQmlTypes()` з
  `cutefish-framework/applications` (підтягується через відносний
  `add_subdirectory(../cutefish-framework/...)` — працює в нашому layout,
  де `settings/` лежить поруч із `cutefish-framework/`).
- QML додатково імпортує форк-модулі фреймворку: `Cutefish.Network`,
  `Cutefish.Bluetooth`, `Cutefish.Audio`, `Cutefish.Appearance`,
  `Cutefish.Accounts` — **всі вже встановлені** в `~/.local/lib64/qt6/qml/Cutefish/`.
  Виняток: `Cutefish.Screen 1.0` (сторінка Display) — модуля немає:
  `cutefish-framework/screen` вимагає `KF6Screen`, чий devel-пакет відсутній
  (той самий блокер, що в `statusbar/STEP0-AUDIT.md` §0.7.1). Сторінку лишаємо
  як є (оригінал), фіксуємо як відомий gap кроку 2.
- VPN-сторінка закоментована в `SideBar.qml`, але `src/vpn/vpn.cpp` компілюється
  і лінкується проти `KF6::NetworkManagerQt` + `KF6::ModemManagerQt` — обидва
  devel-пакети **є** на системі (`/usr/lib64/cmake/KF6NetworkManagerQt`,
  `KF6ModemManagerQt`), лишаємо `REQUIRED` без змін.
- `KF6::ConfigCore` прилінковано в CMake, але **жодного використання в `src/`**
  (grep `KConfig|ConfigCore` → 0). Мертвий лінк, що ламає конфігурацію:
  `find_package(KF6Config)` без `REQUIRED` мовчки проходить, а
  `target_link_libraries(... KF6::ConfigCore)` падає (`KF6Config_*` відсутній
  в `/usr/lib64/cmake`). Прибираємо за патерном terminal (`KF6::WindowSystem`
  у dock/terminal).
- `Qt::AA_EnableHighDpiScaling` в `src/` немає (grep → NONE) — нічого чистити.
- Класичні private-prefix проблеми (патерн terminal `777fda9` / launcher):
  - `application.cpp:97` — хардкод `/usr/share/cutefish-settings/translations/`,
  - `m_engine` знає лише `qrc:/` — Qt6 ігнорує `QT_QML_IMPORT_PATH`, без
    `addImportPath(<prefix>/lib64/qt6/qml)` `.desktop`-запуск не знайде
    FishUI/Cutefish-модулі,
  - `CMakeLists.txt` — `install(... /usr/share/...)` абсолютні шляхи (потрібен root),
    `cutefish-settings.desktop` — `Exec=/usr/bin/cutefish-settings`.

---

## 0.1 Вендорені копії — НЕМАЄ ✅

На відміну від terminal (`qmltermwidget`), settings не вендорить жодної
бібліотеки: весь `src/` — власний код + `cutefish-framework/{applications,
appearance}` через `add_subdirectory`. Обсяг порту — лише рантайм-фікси.

---

## 0.2 Wayland / LayerShell — **ПРОПУЩЕНО** ✅

0 X11-збігів (див. таблицю). `FishUI.Window` — звичайний xdg-toplevel,
KWin керує розміром/декором сам. Жодних Wayland-адаптацій у C++ не потрібно.

---

## 0.3 Залежність від fishui/framework — **Runtime-only** ✅

```
qml/main.qml, SideBar.qml, <pages>/Main.qml: import FishUI 1.0 as FishUI
  → FishUI.Window / Theme / WindowBlur / ...
src/*.cpp: 0 #include <FishUI/...>
```

C++ лінкується лише проти `Cutefish::Applications`, `Cutefish::Appearance`,
Qt6, KF6 NetworkManager/ModemManager, FontConfig, ICU, Freetype, Libcrypt —
усі знаходяться на системі. QML-залежності закриває локальний Qt6 fishui +
вже встановлені форк-модулі фреймворку.

---

## Підсумок аудиту (для кроку 1)

**Обсяг роботи**: малий (менший за terminal — вендореного коду немає, Qt6
вже зроблено upstream). Крок 1 = terminal-набір `777fda9`:

1. CMake: `GNUInstallDirs` (bindir/datadir замість `/usr/share`), прибрати
   мертвий `KF6::ConfigCore` (+ `find_package(KF6Config)`), `Exec=cutefish-settings`.
2. `application.cpp`: переклади через `QStandardPaths::locateAll`, 
   `m_engine.addImportPath(applicationDirPath()+"/../lib64/qt6/qml")`.
3. Збірка в `/tmp/opencode/build-settings`, інсталяція в `~/.local`,
   запуск встановленого бінарника без env-костилів.
4. Відомий gap: сторінка Display (`Cutefish.Screen`, потрібен `KF6Screen`-devel).

Формат звіту кроку 1: той самий, що в terminal — `STEP2-SETTINGS-WAYLAND.md`,
коміти по логічних шматках.
