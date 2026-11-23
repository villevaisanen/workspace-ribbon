# Workspace Ribbon test report

## 0.2.0 development integration

- Release CMake build with the installed Hyprland 0.56.2 headers and CTest passed.
- Manifest validation passed in a clean combined checkout. Omarchy's validator
  rejects the pre-existing symlinks in the development checkout's ignored
  `.tools/` environment, which is excluded from the clean package.
- `qmlformat`, `qmllint` with Omarchy's modules, and the three isolated
  `tests/settings.py` tests passed. They cover live values, bounds, invalid
  inputs, atomic persistence and missing or competing native loaders.
- In the existing Omarchy session the combined panel opened from `/tmp` after
  temporarily repointing the single plugin symlink. It displayed the active
  six values, saved state and existing Lua loader. The native plugin handle
  stayed the same. The original panel link was restored after the check.
- In a separate nested Hyprland with temporary XDG paths, the helper detected
  the missing native plugin, built and installed 0.2.0, enabled it and verified
  the options. Duplicate loads were rejected. Live edits, Save, managed login
  opt-in/out and unloading were exercised only inside that nested session.
- The first-use button was checked through QML validation and its helper in
  nested Hyprland. No physical login cycle or hyprpm install was exercised.

## Initial report — 2026-11-09

## Verified locally

- CMake Debug and Release builds against installed Hyprland 0.56.2 headers: passed.
- GCC 16.2.1 matches compositor binary compiler metadata.
- Required plugin entry points exported in ELF shared object (nm -D).
- CTest animation model: 1 executable passed in both Debug and Release builds. Checks forward/backward and
  adjacent/distant targets, monotonic ease-out, exact endpoints, rapid retargets,
  interruption continuity, spring position/velocity continuity, distance scaling,
  maximum duration and independent animation instances.
- Cell visibility math for two adjacent cells versus an offscreen cell passed.
- Existing Lua configuration inspected; read-only `hyprctl configerrors` empty.
- One attached monitor observed: eDP-1, 1440×900, scale 1.
- Private GitHub repository visibility verified through gh.

## Not verified

No native plugin was loaded. Therefore Stage B load success, Stage D graphical
output, stability, performance, and unload/reload safety are not certified.
No compositor was restarted, replaced or patched. No user configuration changed.
Model checks do not verify compositor rendering or physical multiple monitors.

## Manual acceptance, requiring approval to load

1. Save work and use the README manual load command; check plugin list and logs.
2. Populate workspaces 1–5 with distinguishable tiled windows. Exercise 2→3,
   2→4, 1→5 and reverse with existing keys; record high-frame-rate video.
3. Leave workspace 3 absent or empty; verify it appears as wallpaper during 1→5.
4. Rapidly alternate targets mid-flight; inspect for discontinuities and stale cells.
5. Check popups, decorations and shadows at seams; current clipping limitation is
   expected and must be fixed before a stable release.
6. Confirm fullscreen, floating, named, special and foreign-monitor paths fall back.
7. Attach a second monitor, test independent transitions, disconnect/reassign a
   workspace mid-flight; verify no foreign windows appear or ownership changes.
8. Verify scale, rotation, resize, VRR, screencopy, session lock and resume.
9. Unload both idle and mid-flight; confirm native workspaceSlide behavior returns.
   Reload and repeat. Check for retained resources and unexpected frame scheduling.
10. Measure CPU/GPU idle versus transitions and allocation/frame behavior. No
    benchmark numbers are currently available.

Do not combine with workspace swipe gestures or workspace-rendering plugins yet.
