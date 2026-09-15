# Крок 2: Settings — Qt6/KF6 Wayland port (Plasma 6)

Гілка: `fork-qt6-wayland` (аудит — `STEP0-AUDIT.md`; код — див. коміти нижче)
Framework: локальний `cutefish-framework` (`appearance` + `applications`,
селективна збірка) — reuse, без змін
FishUI: локальний Qt6-форк (`~/.local/lib64/qt6/qml/FishUI`) — runtime-only, без змін
Upstream base: `de04322` (Qt6, X11-нейтральний; репозиторій НЕ заархівовано —
можливий майбутній PR, як у terminal)
Система: Fedora 44, Plasma 6.7.5, Qt 6.11.2, real Wayland (`XDG_SESSION_TYPE=wayland`)

## Що змінилось («було → стало»)

| Область | Upstream | Порт |
|---|---|---|
| CMake install | `install(... /usr/share/...)` абсолютні шляхи (потрібен root); `${CMAKE_INSTALL_BINDIR}` без `include(GNUInstallDirs)` (порожній на чистій конфігурації) | `include(GNUInstallDirs)`, `DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/`, переклади в `${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations` — патерн terminal `777fda9` |
| `KF6::ConfigCore` | `find_package(KF6Config)` (без REQUIRED) + лінк `KF6::ConfigCore` — **0 використань в `src/`**, конфігурація падала (`KF6Config_*.cmake` відсутній — devel-пакета немає) | прибрано з `find_package`/`target_link_libraries` (патерн terminal: мертвий `KF6::WindowSystem` у dock/terminal) |
| ICU-лінкування | `pkg_search_module(ICU REQUIRED icu-i18n)` + `${ICU_LDFLAGS}` — лінк падав (`UnicodeString` з `libicuuc`, `DSO missing`; `pkg_search_module` приймає лише один модуль, `icu-uc` губився, кеш підтвердив `-licui18n` без `-licuuc`) | `pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-i18n icu-uc)` + `PkgConfig::ICU` (стиль сусіднього `FontConfig`) |
| Запуск встановленого бінарника | `m_engine` знав лише `qrc:/` — Qt6 ігнорує `QT_QML_IMPORT_PATH`, `.desktop`-запуск не знаходив FishUI/Cutefish-модулі | `m_engine.addImportPath(applicationDirPath()+"/../lib64/qt6/qml")` — патерн terminal/launcher/dock |
| Переклади | хардкод `/usr/share/cutefish-settings/translations/` | `QStandardPaths::locateAll(GenericDataLocation, ...)` — патерн terminal/launcher |
| `.desktop` | `Exec=/usr/bin/cutefish-settings` | `Exec=cutefish-settings` (PATH; `~/.local/bin` у PATH) → іконка в сітці додатків |
| RPATH бінарника | немає — `libcutefish-framework-appearance.so` (ставиться лише збіркою фреймворку в `<prefix>/lib64`) не знаходився без `LD_LIBRARY_PATH` | `INSTALL_RPATH "$ORIGIN/../lib64"` (патерн fishui `dd96285`) — запуск без env-костилів |

Wayland-специфіки НЕМАЄ (аудит 0.2): звичайний toplevel (`FishUI.Window`),
LayerShellQt не потрібен. `Qt::AA_EnableHighDpiScaling` в коді немає.
VPN-код (`KF6::NetworkManagerQt`/`ModemManagerQt`) лишено без змін —
обидва devel-пакети є на системі.

## Збірка / встановлення / запуск (private prefix)

```
cmake -S settings -B /tmp/opencode/build-settings \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local
cmake --build /tmp/opencode/build-settings -j$(nproc)
cmake --install /tmp/opencode/build-settings
update-desktop-database ~/.local/share/applications/
```

Результат: `~/.local/bin/cutefish-settings` (RUNPATH `$ORIGIN/../lib64`),
`~/.local/share/applications/cutefish-settings.desktop`,
переклади в `~/.local/share/cutefish-settings/translations/`.
`cutefish-framework-applications` лінкується статично (`.a` в дереві збірки,
інсталяції не потребує); `appearance` `.so` — із ранішої селективної збірки
фреймворку. Запуск — з сітки додатків або `~/.local/bin/cutefish-settings`,
БЕЗ env-костилів.

## Перевірено на Plasma 6.7.5 (Fedora 44, Wayland)

- Збірка чиста (депрекейшн-варнінги `qAsConst`/`QAbstractItemModel`; фаталів немає),
  інсталяція в `~/.local`.
- Запуск встановленого бінарника під `QT_QPA_PLATFORM=offscreen`: процес живе
  (timeout 124 = event loop), stderr порожній — стартова сторінка WLAN грузиться
  без QML-помилок.
- `QML_IMPORT_TRACE=1`: `FishUI` + `Cutefish.Network` резолвляться з
  `~/.local/lib64/qt6/qml` (кодовий `addImportPath`); жодного
  `module "X" is not installed`. (Два `locateLocalQmldir ... not found` —
  штатні промахи локального пошуку: `Cutefish.Settings` реєструється через
  `qmlRegisterType` без qmldir за дизайном.)
- Середовище перевірки не містило наших шляхів (`LD_LIBRARY_PATH` — лише
  AppImage-маунти рантайму, `QT_QML_IMPORT_PATH` порожній) — RUNPATH і
  import-path доведені, а не масковані env.
- Іконка Settings у сітці додатків (після `update-desktop-database`).

## Ключові знахідки

1. **`pkg_search_module` ≠ `pkg_check_modules`**: перший приймає лише ОДИН
   модуль (решта аргументів — обмеження версії), тому `icu-uc` мовчки губився.
   Для двох ICU-модулів потрібен `pkg_check_modules(... icu-i18n icu-uc)`.
2. **`find_package(KF6Config)` без `REQUIRED` — пастка**: конфігурація проходить,
   а генерація падає на `target_link_libraries(KF6::ConfigCore)`. Мертвий лінк
   (0 використань) — видаляти, як `KF6::WindowSystem` у dock/terminal.
3. **`${CMAKE_INSTALL_BINDIR}` без `include(GNUInstallDirs)` — порожній**:
   upstream використовував змінну, не підключивши модуль; на чистому дереві
   `install(TARGETS ... RUNTIME DESTINATION )` мовчки ламався.
4. **Settings — перший споживач framework `.so` у C++**: dock/launcher/terminal
   тягнуть фреймворк лише через QML-плагіни (у них власний RPATH), тому
   відсутність RUNPATH у бінарнику вилізла тільки тут.

## Що лишилось

- Сторінка Display (`Cutefish.Screen 1.0`, `Display/Main.qml:24,35`): модуля
  немає в `~/.local` — `cutefish-framework/screen` вимагає `KF6Screen`
  (devel-пакета немає; той самий блокер, що в `statusbar/STEP0-AUDIT.md` §0.7.1).
  Сторінка грузиться ліниво (5-та в сайдбарі), старт застосунку не зачіпає.
  Варіанти: встановити `kf6-kscreen-devel` і дозібрати модуль, або заглушка.
- Юзер-верифікація кліком у GUI (фінальний чекпоінт, як у terminal):
  відкрити з сітки, поклікати сторінки (WLAN/Bluetooth/Appearance/Display),
  перевірити темну тему через FishUI.
