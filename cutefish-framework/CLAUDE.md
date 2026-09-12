@../CLAUDE.md

# CutefishOS libcutefish → cutefish-framework — Qt6/KF6 порт

## Збірка

```
cmake -S cutefish-framework -B /tmp/opencode/build-framework -DCMAKE_INSTALL_PREFIX:PATH=$HOME/.local
cmake --build /tmp/opencode/build-framework -j$(nproc)
cmake --install /tmp/opencode/build-framework
```

## Що тут живе

Фреймворк (колишній `libcutefish`). Dock використовує **тільки**
`appearance`:

- `appearance/appearance.cpp` — `Appearance::darkMode()`; коли daemon
  недоступний, fallback на `~/.config/cutefishos/appearance.conf`
  (`[General] darkMode=true`).
- Ліба: `libcutefish-framework-appearance.so` → `~/.local/lib64/`
  (щоразу копіюється вручну після збірки fishui-дерева теж — там вона
  збирається SHARED в `cutefish-framework-appearance-build/`).

## Нюанси

- Гілка `fork-qt6-wayland`; коміти локальні (не запушено).
- Daemon (`cutefish-appdaemon`) на Plasma-системі зазвичай НЕ запущений —
  саме тому важливий локальний fallback на конфіг.