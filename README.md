# workspace-ribbon

Continuous horizontal workspace animation and an Omarchy settings panel in one project.

Development version **0.2.0** on `dev`. The native plugin targets Hyprland 0.56.2.
See [architecture](docs/architecture.md), [compatibility](docs/compatibility.md),
and [test report](docs/testing.md). Licensed GPL-3.0-only.

This prototype renders intermediate workspaces with tiled and ordinary floating windows using one time-based
viewport coordinate. It does not chain workspace dispatches. Fullscreen, pinned, special and
foreign-monitor paths fall back to native behavior. Floating windows that
cannot be isolated within a cell use native behavior only on affected paths. Popup/shadow seam
clipping is incomplete. The zoom prototype has been loaded on Omarchy with
Hyprland 0.56.2. Full graphical validation is pending.

## Move from the separate local panel to this checkout

The shell plugin keeps its current ID, so Omarchy can reuse its enabled state
and any existing shortcuts. On this machine the existing plugin directory is
a symlink to the old project. Run these commands to replace that single link:

```sh
omarchy-shell shell hide villevaisanen.workspace-ribbon
ln -sfnT /home/air/workspace-ribbon /home/air/.config/omarchy/plugins/villevaisanen.workspace-ribbon
sleep 1
OMARCHY_SHELL_IPC_TIMEOUT=10s omarchy-shell shell rescanPlugins
omarchy-shell shell summon villevaisanen.workspace-ribbon '{}'
```

Use the `ln` command only when the destination is your current symlink. If you
installed a directory there instead, move that directory aside first. The old
local source remains in `~/dev/omarchy-workspace-ribbon-settings`. To switch back,
run the same commands with that directory as the link target. Keep the existing
native loader and saved Lua settings. The panel uses the loaded 0.1.1 build or
any build that exposes all six compatible settings.

## Install the combined package as a beta tester

For the current local `dev` snapshot, use the checkout owner’s path as the
symlink target. A beta tester with a shared copy of the branch can do this from
any directory:

```sh
repo="$HOME/workspace-ribbon"
link="$HOME/.config/omarchy/plugins/villevaisanen.workspace-ribbon"
omarchy plugin validate "$repo"
mkdir -p "$(dirname "$link")"
ln -s "$repo" "$link"
omarchy-shell shell rescanPlugins
for attempt in $(seq 1 20); do
  omarchy plugin list --json | jq -e 'any(.[]; .id == "villevaisanen.workspace-ribbon")' >/dev/null && break
  sleep 0.2
done
omarchy plugin enable villevaisanen.workspace-ribbon
omarchy-shell shell summon villevaisanen.workspace-ribbon '{}'
```

Use a clean checkout for manifest validation. The validator rejects symlinks
inside the checkout, including those created by a Python virtual environment
such as `.tools/`. Keep tool environments outside clean beta checkouts.

Once the combined project reaches the remote default branch, this command
will install and enable the panel for users whose Git credentials can access
the repo. It currently points to the foundation branch and is for that future
release, not the local development snapshot:

```sh
omarchy plugin add https://github.com/villevaisanen/workspace-ribbon.git --enable
```

The helper requires Python 3.11+ and `hyprctl`. Open the panel and click
**Build & enable Workspace Ribbon** when the native plugin is absent.
The helper checks the running Hyprland version and ABI
against the installed headers, then runs CMake's dependency checks and compiles
in `${XDG_CACHE_HOME:-$HOME/.cache}/workspace-ribbon`. The build log is
`build.log` in that directory. It installs the binary under
`${XDG_DATA_HOME:-$HOME/.local/share}/workspace-ribbon/<abi>/<build-id>/`.
Each build has its own path, so it can be installed while an older binary is
mapped. The initial load applies to the current session only.

**Load at login** adds or removes a marked block in the standard Omarchy
`hypr/autostart.lua`. It is available for a panel-activated build. Existing
Lua loaders, enabled hyprpm entries and unowned session loads stay under their
original management. The helper checks literal loaders in Hyprland Lua and
legacy config and the hyprpm state store. Custom Lua loaders and external
startup scripts need manual coordination. Use only one native loader.

**Save** writes the live settings to `workspace-ribbon.lua`. To apply them
at startup, add this line near the end of `hyprland.lua`, after your
`hypr.autostart` import and any existing Ribbon settings:

```lua
dofile((os.getenv("XDG_CONFIG_HOME") or (os.getenv("HOME") .. "/.config")) .. "/hypr/workspace-ribbon.lua")
```

Run `hyprctl reload` and `hyprctl configerrors` after adding the line. Click
inside the panel to focus it, use Tab to navigate and the arrow keys to adjust
zoom and duration. The panel stays open when you switch workspaces. Live
adjustments take effect without a reload command.

To turn off panel-managed startup loading, clear **Load at login** or run
`"$HOME/workspace-ribbon/bin/ribbon-settings" login off`. A plugin loaded by
the Lua entry can unload when Hyprland reloads the edited config. For a
session-only build, use `"$HOME/workspace-ribbon/bin/ribbon-settings" unload`
after removing the startup entry. For an external loader, remove the original
Lua instruction or disable the hyprpm entry using its own tool. Remove the
saved-settings `dofile` line before deleting the saved Lua file.

To remove the panel, run `omarchy plugin disable villevaisanen.workspace-ribbon`.
For a local link, then run
`unlink "$HOME/.config/omarchy/plugins/villevaisanen.workspace-ribbon"` and
`omarchy-shell shell rescanPlugins`. Remove managed startup loading and confirm
the native plugin has unloaded before removing its artifacts from the data directory.

When upgrading, the panel continues to use a compatible loaded version. Run
`"$HOME/workspace-ribbon/bin/ribbon-settings" build --prepare` to compile a
replacement and print its path without loading it. For Lua-managed installs,
replace the old loader path with the new one and reload. For a panel-managed
or manual session-only install, remove its startup entry if present, unload
the old plugin, then use **Build & enable** again. For hyprpm, continue to use
hyprpm. The setup helper never hot-swaps a loaded native plugin automatically.

## Build and load the native plugin manually

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

This machine uses a project-local CMake in the existing ignored `.tools/` directory.
For a clean checkout, create a virtual environment outside the project if system
CMake is unavailable, then put its `bin` directory on your PATH:

```sh
python -m venv "$HOME/.cache/ribbon-build-tools"
"$HOME/.cache/ribbon-build-tools/bin/pip" install cmake==4.4.4
export PATH="$HOME/.cache/ribbon-build-tools/bin:$PATH"
```

## Enable and disable

Use these commands if you manage the native plugin directly. Loading runs
experimental C++ inside the compositor.

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
`bin/ribbon-settings set enabled 0`.
After a manual Lua edit, run `hyprctl reload` and `hyprctl configerrors`.

## Recovery

If loading crashes Hyprland, log in through a TTY (Ctrl+Alt+F3), remove any plugin
startup line you added, or disable it with hyprpm before starting a new session.
Manual loading above is not persistent. Do not repeatedly load a failing build.
Record `hyprctl version`, compiler version and crash logs/coredump before rebuilding.
Use the normal Omarchy login/session process; no compositor restart is automated.

## Development

`dev` carries 0.2.0 locally. `v0.1.1` remains the previous development
snapshot. The `main` branch is reserved for stable work.
Run model-only checks without compositor headers using
`cmake -S . -B build-model -DRIBBON_BUILD_PLUGIN=OFF` then build and ctest.
Run the panel helper's isolated tests with `python tests/settings.py`.
See [test report](docs/testing.md) for the integration results and pending checks.
