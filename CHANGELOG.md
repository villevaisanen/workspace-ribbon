# Changelog

## 0.2.1 — Development

- Support Omarchy workspace layouts and ordinary floating windows during Ribbon slides, preserving native stacking and the pinned-window fallback.

## 0.2.0 — Development

- Integrated the existing Omarchy layer-shell settings panel and live settings helper.
- Added session-only native build and activation, ABI checks, versioned user artifacts and opt-in login loading.
- Existing compatible native builds keep serving the panel without another load.

## 0.1.1 — Development snapshot

- Added configurable center zoom during workspace slides, with a short zoom lead on fresh transitions and immediate motion on retargeting.
- Investigated Omarchy 4.0.6 and Hyprland 0.56.2 rendering architecture.
- Added CMake native plugin target and hyprpm manifest without speculative pins.
- Added steady-clock cubic, linear and spring interpolation with interruption support.
- Added event-based tiled-workspace rendering and conservative native fallbacks.
- Added independent animation tests, model CI and Omarchy installation/recovery docs.
- Added ABI guard and cleanup on unload, reload, monitor removal and workspace moves.
- Verified native loading of the zoom prototype on Hyprland 0.56.2. Full graphical validation is pending.
