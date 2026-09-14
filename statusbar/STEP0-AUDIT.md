# Крок 0 — Аудит: порт CutefishOS statusbar на Qt6/KF6 + Plasma 6 Wayland

Дата: 2026-09-13
Система: Fedora 44 (KDE Plasma Desktop), Plasma 6.7.5, Qt 6.11.2, сесія — real Wayland
(`XDG_SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-0`), KWin 6.7.5.

---

## 0.0 Важлива передумова: стан upstream

CutefishOS statusbar заархівований, але `main`-гілка вже містить **частковий Qt6-порт**:

| Комміт | Що зроблено |
|---|---|
| `d4d467c fix(statusbar): port statusbar to Qt6` | CMake переведено на Qt6/KF6 (`find_package Qt6/KF6WindowSystem`), додано `compat/QX11Info` шим, QML імпорти на `Qt5Compat.GraphicalEffects`. Працювало (20 файлів). |
| `0008af8 fix(debian): update packaging for Qt6 and KF6` | debian/control + rules під Qt6/KF6. |
| `e4b9a61 ci: use Ubuntu 26.04` | CI оновлено. |

**Але весь X11-специфічний C++ залишився неторканим** — той самий патерн, що й у dock:
KX11Extras, NETWM, xcb_change_property/xcb_get_property, NETRootInfo, KWindowInfo.

framework та fishui у локальних форках вже повністю портовані на Qt6/KF6/Wayland.

---

## 0.1 Як statusbar резервує верхній край (ПИТАННЯ 1)

**Відповідь: повністю X11-only — йде на заміну (NET::Dock + _NET_WM_STRUT_PARTIAL).**

Ланцюг у `statusbar.cpp`:

| Рядок | X11-конструкція | Wayland-заміна |
|---|---|---|
| 48 | `KX11Extras::setOnDesktop(winId(), NET::OnAllDesktops)` | не потрібно (Wayland немає desktop-парадигми) |
| 49 | `KX11Extras::setType(winId(), NET::Dock)` | LayerShellQt: `zwlr_layer_shell_v1::layer::top`, `anchor::top` |
| 107 | `setGeometry(QRect(rect.x(), rect.y(), rect.width(), 25))` | розмір визначається LayerShell з `exclusive_zone` |
| 120-137 | `updateViewStruts()` — ручне обчислення `NETExtendedStrut` (top_width = height + topOffset, top_start = x, top_end = x + w) → `KX11Extras::setExtendedStrut(winId(), ...)` | LayerShellQt: `exclusive_zone = висота_бару` (KWin автоматично резервує top area) |
| 143 | `KX11Extras::setState(winId(), KeepAbove/KeepBelow)` залежно від launchPad | LayerShellQt: `layer::top` (KeepAbove) / `layer::bottom` (KeepBelow), або деактивація |
| 111 | `KWindowEffects::enableBlurBehind(this, true)` | KF6 Wayland: `KWindowEffects::enableBlurBehind` працює через `org_kde_kwin_blur` protocol — **лишити без змін** |
| 59-63 | `setSource(...)`, `setResizeMode`, `setScreen(primaryScreen())`, `updateGeometry()`, `setVisible(true)` | зберегти, але після `setSource` (pattern launcher: showMaximized після setSource) |

**Висота бару: 25 / qApp->devicePixelRatio()** — фіксована.

Для LayerShellQt рекомендується:
- `anchors = Top | Left | Right`
- `exclusive_zone = 25` (або висота × dpr)
- `layer = Top`
- `size.width = screen_width`, `size.height = 25`

---

## 0.2 Чи бар показує активне вікно / appmenu (ПИТАННЯ 2)

**Відповідь: ТАК — дві великі підсистеми, обидві повністю X11-only.**

### 0.2.1 Activity — назва/іконка активного вікна

`activity.cpp` надає QML-властивості `title`, `icon`, `launchPad` для панелі.

| Рядки | X11-конструкція | Wayland-заміна |
|---|---|---|
| 43-45 | `KX11Extras::activeWindowChanged` signal, `KX11Extras::windowChanged` | `KWindowSystem::self()->activeWindowChanged()` — працює на Wayland (KF6 KWindowSystem → KWayland plugin) |
| 159-163 | `KWindowInfo(KX11Extras::activeWindow(), NET::WMState | NET::WMVisibleName | NET::WMWindowType, NET::WM2WindowClass)` | `KWindowSystem::activeWindow()` + `KWindowInfo` на Wayland |
| 65 | `NETRootInfo(QX11Info::connection(), NET::CloseWindow).closeWindowRequest(KX11Extras::activeWindow())` | `KWindowSystem::self()->closeWindow(wid)` |
| 70 | `KX11Extras::minimizeWindow(KX11Extras::activeWindow())` | `KWindowSystem::self()->minimizeWindow(wid)` |
| 75-80 | `KX11Extras::clearState/setState(..., NET::Max)` | `KWindowSystem::self()->setShowingDesktop(false)` або `PlasmaWindow::setMaximized/unmaximize` |
| 85-93 | `KWindowInfo(activeWindow, NET::WMState).hasState(NET::Max)` | `KWindowInfo(wid, NET::WMState)` — KWindowInfo кросплатформний на KF6 Wayland |
| 97-117 | `move()` — `NETRootInfo(QX11Info::connection(), NET::WMMoveResize).moveResizeRequest(...)` — пряме X11 move resize | Wayland: `QWindow::startSystemMove()` |
| 120-155 | `isAcceptableWindow()` — `KWindowInfo + NET::WindowTypeMask` — повністю X11-only фільтр | `KWindowInfo` працює на Wayland, але деякі `NET::WMState` flag-и можуть не підтримуватись; фільтрація спрощується (Wayland не має SkipTaskbar/SkipPager тих самих семантик) |

**launchPad detection** (рядки 157-164): `KWindowInfo.windowClassClass() == "cutefish-launcher"` — працює на Wayland через KWindowSystem KWayland plugin (readable WM_CLASS).

### 0.2.2 AppMenu — глобальне меню (macOS-стиль)

Найскладніша X11-only підсистема. Складається з 5 файлів:

| Файл | X11-конструкція | Wayland-заміна |
|---|---|---|
| `appmenumodel.cpp:45-72` | `getWindowPropertyString(activeWindow, "_KDE_NET_WM_APPMENU_OBJECT_PATH/SERVICE_NAME")` — `xcb_get_property` | `org_kde_kwin_appmenu` Wayland protocol — KWin сам обслуговує mappping вікна → appmenu |
| `appmenu.cpp:98-100` | `xcb_connect` fallback, `xcb_change_property` для `_KDE_NET_WM_APPMENU_*` на registered window | `KWayland::Client::AppMenuManager::registerSurface(surface)` або `org_kde_kwin_appmenu` protocol |
| `appmenu.cpp:123-161` | `slotWindowRegistered()` — xcb_change_property на WId | Wayland: зареєструвати surface через KWin appmenu manager protocol |
| `appmenumodel.cpp:101-104` | `KX11Extras::activeWindowChanged` → `onActiveWindowChanged` → читає _KDE_NET_WM_APPMENU_* | `KWindowSystem::self()->activeWindowChanged()` на KF6 + KWindowInfo |
| `menuimporter.cpp:64-83` | `RegisterWindow(WId)` — `KWindowInfo(id, NET::WMWindowType)` — фільтрація типу | Wayland: organizer уникає реєстрації через X11 property; KWin використовує org_kde_kwin_appmenu protocol |
| `appmenuapplet.cpp:156-170` | Вже **кросплатформний**! є `KWindowSystem::isPlatformWayland()` перевірки для popup timing | Можна лишити |

**На Wayland replacement:**
- KDE Plasma використовує `org_kde_kwin_appmenu` Wayland protocol для глобальних меню
- Реалізація: `KWayland::Client::AppMenuManager` (KF6) + `KWayland::Client::AppMenu` (для surface)
- Або через KWindowSystem KF6 API: `KWindowSystem::setApplicationMenu(wid, service, path)` (якщо підтримується)

**Статус:** AppMenu-підсистема — найбільш складна для порту. Можна тимчасово вимкнути (return early, не показувати меню) на Wayland до реалізації через KWin appmenu protocol, або зберегти лише menuimporter + appmenuapplet (DBusMenu сумісний) та прибрати X11 property set/get.

---

## 0.3 SystemTray/StatusNotifierItem (ПИТАННЯ 3)

**Відповідь: повністю кросплатформний DBus — НЕ ЧІПАТИ.**

| Файл | Опис |
|---|---|
| `statusnotifieritemhost.cpp` | SNI Host — реєструється як `org.kde.StatusNotifierHost-<pid>`, підписується на `org.kde.StatusNotifierWatcher`, приймає registered/unregistered items — чисто DBus |
| `statusnotifierwatcher.cpp` | SNI Watcher —DBus-адаптер для реєстрації items/host |
| `statusnotifieritemsource.cpp` | SNI Source — DBus proxy для конкретного іконки (icon, tooltip, context menu, activate) — чисто DBus |
| `systemtraymodel.cpp` | QAbstractListModel з `itemAdded/Removed` signal. **`#include <KX11Extras>` є (рядок 26) але НЕ використовується функціонально** — 1 ref = сам include |

На Wayland SNI працює аналогічно (KWin/SNI Watcher виступає проміжним через DBus). Немає legacy XEmbed-трею — лише сучасний SNI.

---

## 0.4 Годинник/дата (ПИТАННЯ 4)

**Відповідь: кросплатформно — немає платформозалежностей.**

- `main.qml:42-46` — `StatusBar.twentyFourTime` з `QSettings("cutefishos", "locale")` (statusbar.cpp:42-43)
- `main.qml:401-408` — `Timer { id: timeTimer; interval: 1000; running: true; repeat: true; onTriggered: timeLabel.text = new Date().toLocaleTimeString(Qt.locale(), format) }`
- Чисто QML + QLocale — Wayland-незалежно.

---

## 0.5 FishUI компоненти (ПИТАННЯ 5)

QML statusbar імпортує `FishUI 1.0 as FishUI` та використовує:

| FishUI тип | Де використовується | Наявний у fishui fork? |
|---|---|---|
| `FishUI.WindowHelper` | main.qml:90 | ✓ (`src/platforms/linux/windowhelper.cpp`) |
| `FishUI.PopupTips` | main.qml:94 | ✓ (`src/controls/PopupTips.qml`) |
| `FishUI.DesktopMenu` | main.qml:98 | ✓ (`src/controls/DesktopMenu.qml`) |
| `FishUI.Units` | main.qml:110+ (smallSpacing, largeSpacing) | ✓ (`src/controls/Units/Units.qml` — ιмовірно) |
| `FishUI.Theme` | (commented out, але може знадобитись) | ✓ (`src/controls/Theme/Theme.qml`) |
| `FishUI.IconItem` | SystemTray.qml:105 | ✓ (`src/iconitem.cpp`) |
| `FishUI.WindowBlur` | ControlCenter.qml:122, ShutdownDialog.qml:52 | ✓ (`src/platforms/linux/blurhelper/windowblur.cpp`) |
| `FishUI.WindowShadow` | ControlCenter.qml:129, ShutdownDialog.qml:59 | ✓ (`src/platforms/linux/shadowhelper/windowshadow.cpp`) |

**FishUI fork має всі компоненти.** FishUI URI `FishUI 1.0` зберігається.

---

## 0.6 Framework модулі (ПИТАННЯ 6)

QML statusbar імпортує 7 Cutefish-модулів. Fork framework має їх під **перейменованими URI**:

| Upstream statusbar імпорт | Fork URI | Типи (статусбар → fork) | Дія |
|---|---|---|---|
| `Cutefish.System 1.0 as System` | `Cutefish.Appearance` | `System.Wallpaper { type; path; color }` → `Appearance.Wallpaper` | ✱ Перейменувати import → `Cutefish.Appearance 1.0 as Appearance`, QML → `Appearance.Wallpaper` |
| `Cutefish.NetworkManagement 1.0 as NM` | `Cutefish.Network` | `NM.ActiveConnection`, `NM.EnabledConnections`, `NM.Handler` → ті самі типи | ✱ Перейменувати import → `Cutefish.Network 1.0 as NM` (alias лишити) |
| `Cutefish.Accounts 1.0 as Accounts` | `Cutefish.Accounts` | `Accounts.UserAccount` | ✓ Збігається |
| `Cutefish.Bluez 1.0 as Bluez` | `Cutefish.Bluetooth` | `Bluez.Manager.bluetoothBlocked`, `Bluez.Manager.adapters` → fork `BluetoothManager` (інший API!) | ⚠️ Fork bluetooth applet має інший API (connectToDevice, requestParingConnection, без bluetoothBlocked). Потребує або розширення BluetoothManager додавши `bluetoothBlocked/adapters` Q_PROPERTY, або відмову від цієї функціональності на Wayland, або використати KF6 KBluetooth модуль. Детальний аналіз — крок 1. |
| `Cutefish.Audio 1.0` | `Cutefish.Audio` | `PulseAudio.NormalVolume`, `PulseAudio.MinimalVolume`, `PulseAudio.SinkModel` → fork `PulseAudio` singleton з тим самим API | ✓ Збігається |
| `Cutefish.Mpris 1.0` | `Cutefish.Media` | `Mpris.Playing`, `Mpris.metadataToString`, `MprisPlayer` → fork має `Mpris` singleton з тим самим API | ✱ Перейменувати import → `Cutefish.Media 1.0 as Mpris` (або без alias) |
| `Cutefish.StatusBar 1.0` | — (свій модуль) | SystemTrayModel, ControlCenterDialog, AppMenuModel, Appearance, Battery, Brightness, etc. | ✓ Свій модуль — лишається |

**Також:** upstream libcutefish мав `Cutefish.System` — fork перейменував на `Cutefish.Appearance`. Але终端 (launcher fork) використовує `Appearance.Wallpaper` → стабільний API.

**Статус蓝牙:** Fork `Cutefish.Bluetooth` має `BluetoothManager` з іншим API (bluezqtextensionplugin.cpp реєструє `BluetoothManager`), без Q_PROPERTY `bluetoothBlocked`/`adapters`. ControlCenter.qml використовує ці властивості для toggle Bluetooth ON/OFF та показу адаптерів. Можливо, `BluetoothManager` не основний менеджер — у fork може бути інший клас. Рекомендація: визначити на кроці 1, чи додати ці властивості, чи використати KF6 KBluetooth модуль.

---

## 0.7 Множинні монітори (ПИТАННЯ 7)

**Відповідь: статус-бар з'являється на primaryScreen. QScreen API кросплатформний.**

| statusbar.cpp | Опис |
|---|---|
| 61 | `setScreen(qApp->primaryScreen())` |
| 72 | `connect(qGuiApp, &QGuiApplication::primaryScreenChanged, ...)` |
| 68-69 | `connect(screen(), &QScreen::virtualGeometryChanged/geometryChanged, ...)` |
| 100 | `updateGeometry()`: `screen()->geometry()` → `setGeometry(rect.x(), rect.y(), rect.width(), 25)` |
| 146-155 | `onPrimaryScreenChanged()` — disconnect/connect з new screen |

Multi-screen коміти (691df80, 6484635) — фікс позиціонування. На Wayland LayerShell з `anchor::top` + `size.width = screen_width` — KWin автоматично покаже на активному/primary моніторі. Потрібно: залишити `setScreen(primaryScreen)` + `updateGeometry` для випадків, коли primary screen змінюється.

---

## 0.7.1 Build-залежності модулів framework (перевірено на системі)

Fork framework збирається вибірково (`CUTEFISH_FRAMEWORK_BUILD_MODULES`); для dock/launcher
збиралися лише `appearance`. Statusbar потребує більше модулів — перевірено наявність пакетів:

| Модуль форку | Потрібен statusbar? | Зовнішні залежності | Наявність на системі | Висновок |
|---|---|---|---|---|
| `appearance` | System.Wallpaper | ні | ✓ (вже зібрано) | збирається |
| `accounts` | Accounts.UserAccount (ControlCenter/ShutdownDialog) | лише Qt6 DBus | ✓ | збирається |
| `media` (Mpris) | Mpris.* (MprisItem.qml) | лише Qt6 DBus/Quick | ✓ | збирається |
| `audio` | PulseAudio.* / SinkModel (ControlCenter) | libpulse (REQUIRED) + Canberra + SoundThemeFreedesktop | ❌ libpulse MISSING | **не збирається без `pulseaudio-libs-devel`** |
| `bluetooth` | Bluez.Manager (ControlCenter) | KF6BluezQt | ❌ MISSING | **не збирається без `KF6BluezQt-devel`** |
| `network` | NM.* (main.qml network widget) | KF6NetworkManagerQt + KF6ModemManagerQt | ❌ MISSING | **не збирається без devel-пакетів** |

**Висновок для кроку 1:** базовий статус-бар (панель + годинник + трей + appmenu + Mpris)
можна зібрати із `appearance + accounts + media`. ControlCenter-блютуз/звук/мережа потребують
або встановлення devel-пакетів, або тимчасового вимкнення відповідних карток (queuing).

---

## 0.8 Додаткові знахідки

### controlcenterdialog.cpp

- `KX11Extras::setState(winId(), NET::SkipTaskbar | SkipPager | SkipSwitcher)` (рядок 52) — X11-only; на Wayland `Qt::Popup` вікно автоматично не з'являється в taskbar — **прибрати KX11Extras-виклик**.
- `QWindow::setMouseGrabEnabled(true)` / `setKeyboardGrabEnabled(true)` — на Wayland popup grab може потребувати іншого підходу (Qt Wayland plugin вже обробляє popup grabs).

### main.cpp

- `Qt::AA_EnableHighDpiScaling` — deprecated в Qt6, безпечне видалення.
- `qmlRegisterType<Cutefish.StatusBar модулі>` — можна перевести на `qt_add_qml_module`, але необов'язково.
- `QApplication` (не `QGuiApplication`) — потрібно через `QMenu` (appmenu/VerticalMenu). На Wayland `QApplication` працює.

### Переклади

- `main.cpp:54` — hardcoded path `/usr/share/cutefish-statusbar/translations/` — те саме, що було у launcher до фікса.
- Рекомендація: перевести на `QStandardPaths::locate(QStandardPaths::GenericDataLocation, "cutefish-statusbar/translations/")`.

### backgroundhelper.cpp

- Використовує `QPixmap`, `QPixmapCache` — кросплатформні.
- Зчитує шпалеру як QImage → скейлить → crop для кольору панелі. Wayland-незалежно.

---

## Підсумкова таблиця: файли → X11-конструкція → заміна

| Файл | X11-конструкція | Статус | Wayland-заміна |
|---|---|---|---|
| `statusbar.cpp` | `KX11Extras::setOnDesktop/setType(NET::Dock)` | ❌ замінити | LayerShellQt layer::top, anchor::top, exclusive_zone |
| `statusbar.cpp` | `KX11Extras::setExtendedStrut(...)` | ❌ замінити | LayerShellQt exclusive_zone (авто-резервування) |
| `statusbar.cpp` | `KX11Extras::setState(KeepAbove/KeepBelow)` | ❌ замінити | LayerShellQt layer::top / layer::bottom або KWindowSystem |
| `statusbar.cpp` | `KWindowEffects::enableBlurBehind` | ✓ лишити | KF6 Wayland підтримує (org_kde_kwin_blur) |
| `activity.cpp` | `KX11Extras::activeWindowChanged/windowChanged` | ❌ замінити | `KWindowSystem::self()` signals |
| `activity.cpp` | `KX11Extras::minimizeWindow/activeWindow/close/move...` | ❌ замінити | `KWindowSystem::minimizeWindow/closeWindow` + `QWindow::startSystemMove` |
| `activity.cpp` | `NETRootInfo::closeWindowRequest/moveResizeRequest` | ❌ замінити | KWindowSystem API |
| `activity.cpp` | `isAcceptableWindow()` — NET::WindowTypeMask | ⚠️ спростити | KWindowInfo працює на Wayland, але SkipTaskbar/SkipPager семантика інша |
| `controlcenterdialog.cpp` | `KX11Extras::setState(SkipTaskbar|SkipPager|SkipSwitcher)` | ❌ прибрати | Qt::Popup вже не показується в taskbar |
| `appmenu/appmenu.cpp` | `xcb_change_property(_KDE_NET_WM_APPMENU_*)` | ❌ замінити | org_kde_kwin_appmenu Wayland protocol або тимчасово деактивувати |
| `appmenu/appmenumodel.cpp` | `xcb_get_property(_KDE_NET_WM_APPMENU_*)` | ❌ замінити | org_kde_kwin_appmenu protocol / KWindowSystem |
| `appmenu/appmenumodel.cpp` | `KX11Extras::activeWindowChanged` | ❌ замінити | `KWindowSystem::self()` signals |
| `appmenu/menuimporter.cpp` | `KWindowInfo(wid, NET::WMWindowType)` | ⚠️ перевірити | KWindowInfo працює на Wayland, але部分 flag-и можуть бути інакшими |
| `appmenu/appmenuapplet.cpp` | `KWindowSystem::isPlatformWayland()` | ✓ вже кросплатформний | Можна лишити |
| `systemtray/*.cpp` | `#include <KX11Extras>` (не використовується) | ✓ не чіпати | Прибрати include, SNI — DBus |
| `main.cpp` | `Qt::AA_EnableHighDpiScaling` | ✓ видалити | Deprecated в Qt6 |

---

## Рекомендації для кроку 1 (порту)

### Мінімум для запуску (MVP)

1. **LayerShellQt** для `statusbar.cpp`: замінити KX11Extras::setOnDesktop/setType/setExtendedStrut/setState на LayerShellQt (layer top, anchor top, exclusive zone = height).
2. **Activity**: замінити KX11Extras signals/actions на KWindowSystem KF6 API (кросплатформний).
3. **ControlCenterDialog**: прибрати KX11Extras::setState (Qt::Popup вже без taskbar).
4. **CMakeLists**: додати LayerShellQt (target_link_libraries), прибрати КФ5-залежності.
5. **QML imports**: перейменувати `Cutefish.System` → `Cutefish.Appearance`, `Cutefish.NetworkManagement` → `Cutefish.Network`, `Cutefish.Mpris` → `Cutefish.Media`, `Cutefish.Bluez` → `Cutefish.Bluetooth` (+ API-звірка).

### Повна реалізація

6. **AppMenu**: або реалізувати через `org_kde_kwin_appmenu` Wayland protocol, або тимчасово деактивувати (return early), залишивши DBusMenu імпортер для майбутнього.
7. **AppMenu Model**: `getWindowPropertyString()` → KWindowSystem/KWindowInfo або відключити поки protocol не реалізовано.
8. **QML Bluetooth API**: розширити `BluetoothManager` (fork) або адаптувати ControlCenter до нового API.

### Потім

9. `translate path` — QStandardPaths (як launcher).
10. `Qt::AA_EnableHighDpiScaling` — видалити.
11. `desktop/` fallback або повноцінний Wayland-protocol для appmenu.
12. Тестування одночасно з dock (обидва LayerShellQt-клієнти, перевірити exclusive zones зверху + знизу).

---

## Статус кроку 0

✅ Аудит завершено. Усі X11-specific зони виявлено та каталогізовано.

Наступний крок: STEP2-STATUSBAR-WAYLAND.md (портування).
