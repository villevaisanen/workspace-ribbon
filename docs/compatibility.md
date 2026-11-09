# Compatibility

Investigated 2026-11-09 on Omarchy 4.0.6-1 / Hyprland 0.56.2-4.
Build target is exactly pkg-config Hyprland 0.56.2, commit
`efb50993780079460b0cbed1363e2166a2de1d9f`, plugin API 0.1.
Both the local compositor binary and build use GCC 16.2.1.
The running compositor reports older Aquamarine/Hyprutils patch versions than
installed headers, but the compositor ABI string uses matching minor versions.
Initialization checks the complete Hyprland ABI string and refuses mismatches.
That guard reduces risk; it cannot prove internal ABI compatibility.

Only compilation and independent model tests have passed. No version is runtime
certified. No commit pins are advertised. Rebuild after compositor updates and
review upstream changes before loading. CMake rejects other release versions.

| Case | Prototype policy |
| --- | --- |
| Positive numeric local workspaces | Continuous position; actual latest window buffers |
| Absent intermediate IDs | Empty desktop, no workspace creation |
| Existing ID on another monitor along path | Native fallback |
| Fullscreen anywhere on target monitor | Native fallback |
| Mapped floating/pinned window on target monitor | Native fallback |
| Named or special workspace | Native fallback |
| Super+number, next/previous, wheel | Existing workspace event, no rebind |
| Swipe gestures | Not integrated; native gesture offsets may conflict; do not combine yet |
| Monitor removal/workspace reassignment | Cancel and release retained state |
| Monitor resize/rotation/scale | Logical width sampled per frame; runtime unverified |
| Independent monitors | Separate state; physical multi-monitor test pending |
| Other plugins modifying workspace offsets/force flags | Unsupported combination |
| Direct scanout, VRR, screen capture, lockscreen | Runtime validation pending |

Ordinary tiled surfaces retain native rendering, decoration, blur and monitor
clipping. Per-workspace seam clipping remains incomplete for shadows/popups.
Do not describe this prototype as a finished replacement for all native animations.
Numeric gaps belonging to disconnected monitors or only configured by rules cannot
be inferred from existing workspace objects; those gaps appear empty.

GPL-3.0-only applies to project source. No upstream implementation code is vendored.
Hyprland itself uses BSD-3-Clause; redistributed upstream code must retain its
notices. Distributors must satisfy licenses of linked dependencies and provide
corresponding GPL source when distributing this plugin. Private hosting does not
waive obligations for later distribution.
