# workspace-ribbon

Native Hyprland continuous horizontal workspace animation prototype for Omarchy.

Development version **0.1.1**.
See [architecture](docs/architecture.md), [compatibility](docs/compatibility.md),
and [test report](docs/testing.md). Licensed GPL-3.0-only.

This prototype renders intermediate **tiled** workspaces using one time-based
viewport coordinate. It does not chain workspace dispatches. Fullscreen, floating,
special and foreign-monitor paths fall back to native behavior. Popup/shadow seam
clipping is incomplete. The zoom prototype has been loaded on Omarchy with
Hyprland 0.56.2. Full graphical validation is pending.

## Build on Omarchy

Keep the existing Lua config and keybindings. Check `hyprctl version` matches the
headers (`pkg-config --modversion hyprland`). Use the same compiler as Hyprland.
Required build tools: CMake >=3.27, make, C++23 compiler and pkg-config. Dependency
modules: hyprland=0.56.2, libdrm, libinput, libudev, pangocairo, pixman-1,
wayland-server, xkbcommon. Install missing system packages yourself only after
reviewing the package transaction.

```sh
gh repo clone villevaisanen/workspace-ribbon -- --branch dev
cd workspace-ribbon
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

This machine had no CMake or hyprpm command. A project-local CMake was installed
in ignored `.tools/`, without system package changes. Here you can use
`.tools/bin/cmake` and `.tools/bin/ctest`. Reproduce that setup if needed with:

```sh
python -m venv .tools
.tools/bin/pip install cmake==4.4.4
```

## Enable and disable

Loading runs experimental C++ inside the compositor. Save your work first.
The following are **manual opt-in commands**, not actions performed by this project:

```sh
hyprctl plugin load "$PWD/build/libworkspace-ribbon.so"
hyprctl plugin list
# Unload while the session is healthy:
hyprctl plugin unload "$PWD/build/libworkspace-ribbon.so"
```

Unload restores neutral workspace offsets and removes listeners. Your existing
workspaceSlide animation remains configured as the fallback. Avoid automatic
startup loading until manual load, transitions, unload and reload are verified.
If you later choose persistence, Lua supports
`hl.plugin.load("/absolute/path/to/libworkspace-ribbon.so")`; remove that line to
disable persistent loading. This project does not edit your config.

Use one loading method and one build path per session. If a Lua load is already
configured, replace its existing path with the new build's absolute path and
run `hyprctl reload`. Hyprland 0.56.2 unloads the previously configured path
before loading the new one. A manual load from a different path while the
plugin is active fails because both copies register the same option names.
After switching to a zoom build, confirm that
`hyprctl getoption plugin:workspace_ribbon:zoom_out_percent` returns an integer.

A valid `hyprpm.toml` is supplied. If a compatible hyprpm is available, upstream
0.56.2 supports a git revision after the URL:

```sh
hyprpm add https://github.com/villevaisanen/workspace-ribbon dev
hyprpm enable workspace-ribbon
# Reverse:
hyprpm disable workspace-ribbon
```

hyprpm has not been tested here and may need writable header storage/build tools.
Private repository access must already work through Git authentication; do not put
tokens in repository URLs. Use `dev`: `main` contains the foundation only.

## Configuration

After loading, options are available through Hyprland's plugin config namespace.
Example Lua, to add manually only after choosing to enable the plugin:

```lua
hl.config({
  plugin = {
    workspace_ribbon = {
      enabled = 1,
      duration_ms = 400,
      max_duration_ms = 1200,
      scale_distance = 0,
      easing = 1,
      zoom_out_percent = 8,
    },
  },
})
```

| Option | Default | Meaning |
| --- | --- | --- |
| enabled | 1 | 0 disables; 1 enables |
| duration_ms | 400 | Base duration, range 1–10000 ms |
| max_duration_ms | 1200 | Duration cap, range 1–10000 ms |
| scale_distance | 0 | 1 multiplies duration by distance, minimum multiplier 1 |
| easing | 1 | 0 linear, 1 cubic ease-out, 2 critically damped spring |
| zoom_out_percent | 8 | Maximum zoom out, 0–25. At 8 the window ribbon reaches 92% scale. Set 0 for the original slide. |

At the start of a fresh transition, the ribbon zooms out around the output
center for 15% of its configured duration, capped at 70 ms. Horizontal motion
then starts, and the ribbon returns to full size in the latter part of the slide.
A target change during the transition continues from the current scale and
position without a new delay. Layer-shell bars, wallpaper and the cursor keep their size.
Zoom uses an internal OpenGL texture draw hook for clipping. If that hook is
unavailable, the original ribbon slide is used.

Spring dynamics use an analytic response with a fixed damping relationship; there
are no separate stiffness/damping controls yet. Ease-out interruptions preserve
position; spring interruptions preserve position and velocity. Finite spring
completion has a small residual correction. Any config reload cancels a transition.
For temporary disable while loaded:
`hyprctl keyword plugin:workspace_ribbon:enabled 0`.
After a manual Lua edit, run `hyprctl reload` and `hyprctl configerrors`.

## Recovery

If loading crashes Hyprland, log in through a TTY (Ctrl+Alt+F3), remove any plugin
startup line you added, or disable it with hyprpm before starting a new session.
Manual loading above is not persistent. Do not repeatedly load a failing build.
Record `hyprctl version`, compiler version and crash logs/coredump before rebuilding.
Use the normal Omarchy login/session process; no compositor restart is automated.

## Development

`v0.1.1` marks the development snapshot on `dev`. The `main` branch is reserved
for stable work. Stable releases require runtime acceptance.
Run model-only checks without compositor headers using
`cmake -S . -B build-model -DRIBBON_BUILD_PLUGIN=OFF` then build and ctest.
See [test report](docs/testing.md) for the remaining acceptance matrix.
