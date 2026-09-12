@../CLAUDE.md

# CutefishOS FishUI — Qt6/KF6 порт

## Збірка/встановлення

```
cmake -S fishui -B /tmp/opencode/build-fishui -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/build-fishui -j$(nproc)
cmake --install /tmp/opencode/build-fishui
# appearance.so інсталер може не ставити — синхронізувати вручну:
cp /tmp/opencode/build-fishui/cutefish-framework-appearance-build/*.so ~/.local/lib64/
```

## Ключові файли

- `src/CMakeLists.txt` — target `FishUI`, `INSTALL_RPATH "$ORIGIN/../../.."` —
  ліби під `~/.local/lib64` знаходяться без `LD_LIBRARY_PATH` (автостарт).
- `src/thememanager.cpp` — тема; dock бере `FishUI.Theme.darkMode`.
- `src/controls/` — QML-контроли (PopupTips, WindowHelper, WindowShadow,
  WindowBlur). У dock використовуються: WindowHelper, WindowShadow, WindowBlur,
  PopupTips, Theme.

## Нюанси

- `cmake --install` fishui ставить QML-модуль у `${INSTALL_QMLDIR}/FishUI` =
  `~/.local/lib64/qt6/qml/FishUI`; `libFishUI.so` — плагін модуля.
- Dup-ліб appearance у `lib64` і в дереві QML не тримати (плутанина типів).