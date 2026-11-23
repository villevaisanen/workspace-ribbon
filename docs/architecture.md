# Architecture and feasibility

## Evidence

Investigated installed Omarchy 4.0.6-1, Hyprland package 0.56.2-4,
upstream tag v0.56.2, commit efb50993780079460b0cbed1363e2166a2de1d9f.
Plugin API reports 0.1; this is not an ABI stability guarantee.

Sources:
- https://wiki.hypr.land/hyprland-plugins/
- https://wiki.hypr.land/hyprland-plugins/development/plugin-guidelines/
- https://omarchy.org/manual/shell-plugins/
- https://github.com/hyprwm/Hyprland/blob/efb50993780079460b0cbed1363e2166a2de1d9f/src/render/Renderer.cpp
- https://github.com/hyprwm/Hyprland/blob/efb50993780079460b0cbed1363e2166a2de1d9f/src/animation/WorkspaceAnimationController.cpp
- https://github.com/hyprwm/Hyprland/blob/efb50993780079460b0cbed1363e2166a2de1d9f/src/output/Monitor.cpp
- https://github.com/hyprwm/Hyprland/blob/efb50993780079460b0cbed1363e2166a2de1d9f/src/event/EventBus.hpp

Omarchy shell plugins are Quickshell/QML components. They can dispatch navigation
but cannot replace compositor rendering. No shell component is needed here.
Native plugin events observe workspace changes after native animations are started.
There is no public event that replaces the workspace renderer or supplies a ribbon
coordinate. Public event registration is used; internal workspace fields supply
render access. The original slide uses event listeners without function hooks. Zoom brackets
the window pass with native renderer hints, keeping shell layers and cursor
outside the transform. One version-specific OpenGL texture draw hook moves
texture geometry and clipping into output coordinates before damage is
computed. Animation frames with zoom bypass untransformed occlusion
simplification. If the texture hook cannot be installed, the slide runs without
zoom. See [Hyprland renderer hints](https://github.com/hyprwm/Hyprland/blob/efb50993780079460b0cbed1363e2166a2de1d9f/src/render/pass/RendererHintsPassElement.hpp).

## Rendering strategy

`renderWorkspaceWindows` collects renderable windows across workspaces, using
`shouldRenderWindow`; `m_forceRendering` permits inactive workspace windows.
`renderWindow` applies each workspace's `m_renderOffset`. We retain existing local
numeric workspaces during a transition, suppress their native offsets/alpha, and
force only cells intersecting the viewport. Missing IDs remain empty wallpaper;
we never create workspaces or transfer ownership. The wallpaper and shell stay fixed.

Each monitor has an independent continuous position p, with workspace n at n−1.
Its offset is (n−1−p) times logical monitor width. Absolute steady-clock time
makes sampling independent of refresh rate. Retargeting samples the old animation
before changing its destination, preserving position. Ease-out may change velocity
on interruption. The optional critically damped spring also preserves velocity.
Only transition frames damage the monitor; no snapshots or plugin GPU allocations.
Existing workspaces are retained by shared reference and released on completion.

## Deliberate prototype restrictions

Fullscreen rendering follows the active workspace's fullscreen branch, so forcing
arbitrary intermediate fullscreen workspaces is insufficient. Floating windows,
pinned windows, and their clipping also have animation-specific logic. Any mapped
floating or fullscreen window on the monitor causes native fallback. Special and
named workspaces use native behavior. Cross-monitor IDs are not adjacency: if an
existing foreign workspace lies in the traversed interval, use native fallback.
For zoom, the ownership check also covers workspaces that could appear at the
widened viewport edges along the path.

Tiled surfaces use Hyprland's existing monitor clipping. Independent per-cell clips
are not implemented: shadows, decorations and popups can spill across cell seams.
This is a known visual limitation, not a claim of complete boundary isolation.
Inactive clients may throttle themselves; rendering uses their actual latest buffers,
not a promise of actively updated video. Focus/input switch immediately to destination,
so clicking during animation follows native destination semantics.

## Alternatives and next work

Hyprexpo (https://github.com/sandwichfarm/hyprexpo) and Hyprspace
(https://github.com/KZDKM/Hyprspace) demonstrate workspace overview approaches.
The official hyprland-plugins repository was inspected for build/config conventions;
its current tree no longer includes hyprexpo. No third-party code was copied.
Offscreen rendering with per-cell clipped passes would support fuller isolation but
requires additional internal renderer integration, careful lockscreen and surface
feedback handling, and likely render hooks. An upstream workspace-render transform
API would be the best long-term solution. Snapshot caching trades live contents and
GPU memory for simpler composition. Sequential dispatch is not implemented.
