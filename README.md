# KDE Cutefish Dock

> **CutefishOS dock ported to Qt6 / KDE Frameworks 6 for Plasma 6 Wayland**
>
> The original CutefishOS look and animations are preserved; every X11-only
> mechanism (NET WM struts, `KX11Extras`, XCB) has been replaced with
> LayerShellQt and KWayland window tracking.  The result is a lightweight,
> always-on-top dock that runs natively under any KWin-based Wayland session.

---

## Features

| Feature | How it works on Wayland |
|---|---|
| Always-visible dock with exclusive screen-edge reservation | LayerShellQt layer-top + exclusive zone |
| Dark theme | `darkMode=true` via local fallback config (the CutefishOS daemon is optional) |
| PWA support — pin any running app to the dock | `desktopPathFromMetadata` matches by Wayland appId first (independent of pid) |
| Drag-and-drop from Kickoff / file manager | Drop `.desktop` files onto the dock to pin them |
| Smooth show / hide (IntellHide mode) | Two-phase: QML opacity fade (200 ms) → geometry resize to ~2px strip (260 ms) |
| Click-to-minimize / click-to-activate | KWayland `PlasmaWindow` `requestToggleMinimized` / `requestActivate` |
| Autostart on login | XDG autostart desktop entry in `~/.config/autostart/` |

---

## Requirements

### Fedora 42+ / RHEL-based (tested)

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtquickcontrols2-devel \
    qt6-qt5compat-devel qt6-qttools-devel \
    extra-cmake-modules \
    kf6-kwindowsystem-devel \
    kwayland-devel \
    layer-shell-qt-devel
```

### Debian 13 / Ubuntu 24.04+ (best-effort — not tested on these distros)

```bash
sudo apt install -y \
    build-essential cmake ninja-build \
    qt6-base-dev qt6-declarative-dev qt6-quickcontrols2-dev \
    qt6-5compat-dev qt6-tools-dev qt6-tools-dev-tools \
    extra-cmake-modules \
    libkf6windowsystem-dev \
    libkwayland-dev \
    layer-shell-qt6-dev
```

> **Note:** Package names may differ between distributions.  If your distro
> uses Qt 6.5+ and KDE Frameworks 6.x, the build should succeed with
> equivalent packages.

---

## Build & install (private prefix `~/.local`)

The project is split into three components.  They must be built **in order**
because `fishui` installs a QML plugin that `dock` loads at runtime, and the
framework's appearance library is needed by both.

### 1. cutefish-framework

```bash
cmake -S cutefish-framework -B build-framework \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-framework -j$(nproc)
cmake --install build-framework
```

### 2. fishui

```bash
cmake -S fishui -B build-fishui \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-fishui -j$(nproc)
cmake --install build-fishui

# The fishui install does not copy the appearance shared library.
# Copy it manually:
cp build-fishui/cutefish-framework-appearance-build/libcutefish-framework-appearance.so \
   "$HOME/.local/lib64/"
```

### 3. dock

```bash
cmake -S dock -B build-dock \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build-dock -j$(nproc)
cp build-dock/cutefish-dock "$HOME/.local/bin/"
```

### 4. Desktop file (KWin permission gate)

KWin requires a desktop file that declares
`X-KDE-Wayland-Interfaces=org_kde_plasma_window_management` before it sends
window management events to the client.  The file lives in
`dock/cutefish-dock.desktop` and must be installed system-wide or under
`~/.local/share/applications/`:

```bash
cp dock/cutefish-dock.desktop "$HOME/.local/share/applications/"
```

### 5. Autostart (optional)

```bash
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/cutefish-dock.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Cutefish Dock
Exec=/home/$USER/.local/bin/cutefish-dock
Terminal=false
X-GNOME-Autostart-enabled=true
X-KDE-autostart-after=plasma-desktop.service
EOF
```

> Replace `$USER` with your actual username in the `Exec` path, or use the
> absolute path directly.

---

## Run

```bash
QT_FORCE_STDERR_LOGGING=1 "$HOME/.local/bin/cutefish-dock"
```

Stop the dock:

```bash
pkill -x cutefish-dock
```

> **Tip:** Set `LD_LIBRARY_PATH=$HOME/.local/lib64` only if the install prefix
> is not `~/.local`.  The fishui QML plugin ships with an `RPATH` that
> resolves the appearance library relative to itself, so the dock works
> without `LD_LIBRARY_PATH` out of the box.

---

## Configuration files

All configuration lives under `~/.config/cutefishos/`.  You can edit them by
hand and restart the dock to apply.

| File | Purpose | Key settings |
|---|---|---|
| `dock.conf` | Dock appearance and behaviour | `Direction` (0=Bottom, 1=Left, 2=Right), `IconSize`, `Style` (0=Round, 1=Rectangular), `Visibility` (0=AlwaysShow, 1=AlwaysHide, 2=IntellHide) |
| `dock_pinned.conf` | Pinned application list | Groups keyed by app id, each with `DesktopPath`, `Exec`, `Icon`, `Index`, `VisibleName` |
| `appearance.conf` | Appearance override when the CutefishOS daemon is not running | `[General] darkMode=true` (or `false` for light panel) |

### D-Bus control

```bash
# Set visibility
busctl --user call com.cutefish.Dock /Dock com.cutefish.Dock setVisibility i 0

# Read back
busctl --user get-property com.cutefish.Dock /Dock com.cutefish.Dock visibility

# Geometry
busctl --user get-property com.cutefish.Dock /Dock com.cutefish.Dock primaryGeometry
```

---

## What changed vs. upstream CutefishOS

| Area | Upstream (Qt5 / X11) | This port (Qt6 / Wayland) |
|---|---|---|
| Panel geometry | `NET::Dock` + extended struts | LayerShellQt layer-top with exclusive zone |
| Window tracking | `KX11Extras` poll of the X root window | KWayland `org_kde_plasma_window_management` |
| Panel surface lifetime | Unmap/remap on show/hide | Permanent mapping: fade + strip resize |
| Keyboard focus | `KeyboardInteractivityOnDemand` | `KeyboardInteractivityNone` (pointer events only) |
| Fade animation | None (geometry snap) | QML opacity transition (200 ms) |
| Desktop file scan | `/usr/share/applications` only | Full XDG path (user, system, flatpak) with priority dedup |
| Desktop file match | `commandFromPid` (X11 WM_CLASS) | Wayland appId first (baseName / StartupWMClass / Icon), then pid fallback |
| Desktop file search | Only `KF6WindowSystem::slideWindow` and `enableBlurBehind` | Removed; slide ineffective with map-forever strip, blur via QML `org_kde_kwin_blur` |
| Build dependencies | `KF6WindowSystem`, `PkgConfig::XCB`, `compat/QX11Info` | `Plasma::KWaylandClient`, `LayerShellQt::Interface` only |

---

## Uninstall

```bash
# Stop the dock
pkill -x cutefish-dock

# Remove installed files
rm -f  "$HOME/.local/bin/cutefish-dock"
rm -f  "$HOME/.local/share/applications/cutefish-dock.desktop"
rm -rf "$HOME/.local/lib64/qt6/qml/FishUI"
rm -f  "$HOME/.local/lib64/libcutefish-framework-appearance.so"
rm -f  "$HOME/.config/autostart/cutefish-dock.desktop"

# Optionally remove config
rm -rf "$HOME/.config/cutefishos"
```

---

## License

This project is released under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)).

### Upstream attribution

The original code is from [CutefishOS](https://github.com/cutefishos/dock)
(repos archived August 2026).  The CutefishOS Team retains copyright over
the original source files; all such files carry the GPL-3.0 header and the
original author line (`rekols <revenmartin@gmail.com>`, `Reion Wong
<reionwong@gmail.com>`).

### Links

- KWin layer-shell: <https://wayland.app/protocols/wlr-layer-shell-unstable-v1>
- KDE slide protocol: `org_kde_kwin_slide` (kf6-kwindowsystem)
- KWayland: <https://invent.kde.org/frameworks/kwayland>
- LayerShellQt: <https://invent.kde.org/plasma/layer-shell-qt>
