# Workspace Ribbon

Workspace Ribbon turns your numbered Hyprland workspaces into a continuous horizontal strip. Jump from workspace 1 to 5 and you see the workspaces in between slide past. The strip zooms out as it moves and returns to full size as you arrive. It works with your existing workspace shortcuts and includes an Omarchy panel for adjusting the animation.

Tiled and ordinary floating windows move with their workspace. Ribbon uses Hyprland’s standard transition for unsupported cases, including fullscreen or pinned windows.

## Installation

Workspace Ribbon 0.2.1 targets **Hyprland 0.56.2**. Check your running version with `hyprctl version`. The local build needs matching Hyprland headers, a compatible C++23 compiler, CMake 3.27 or newer, Make and pkg-config. The panel helper requires Python 3.11 or newer.

1. Install and enable the Omarchy panel. For a private checkout, Git must have access to the repository.

   ```sh
   omarchy plugin add https://github.com/villevaisanen/workspace-ribbon.git --enable
   ```

2. Open the panel. You can use this command again whenever you want to change the settings.

   ```sh
   omarchy-shell shell summon villevaisanen.workspace-ribbon '{}'
   ```

3. Click **Build & enable Workspace Ribbon** to compile and load the animation plugin for this session. If a compatible plugin is already running, the panel shows its settings immediately.

## Usage

Try switching between two non-adjacent workspaces with the panel open. Adjust a control, then repeat the same jump to feel the difference. Changes take effect in the current session.

| Control | What it changes | Default |
| --- | --- | --- |
| Enabled | Turns the Ribbon animation on or off. | On |
| Zoom out | How far the strip recedes. At 8% it shrinks to 92% of its normal size. Set it to 0% for a plain slide. | 8% |
| Duration | Base time for a transition. | 400 ms |
| Motion | Shape of the slide. Choose linear, cubic ease-out or spring. | Ease out |
| Scale duration with distance | Gives longer jumps more time to travel. | Off |
| Maximum duration | Caps the transition time. | 1200 ms |

**To use Ribbon after login**, turn on **Load at login** in the panel. This option is available for builds loaded by the panel.

**To keep your settings**, click **Save**. This writes `~/.config/hypr/workspace-ribbon.lua` (or the equivalent under `$XDG_CONFIG_HOME`). Once, after the first save, add the following line near the end of your `hyprland.lua`, after the `hypr.autostart` import and any existing Ribbon settings:

```lua
dofile((os.getenv("XDG_CONFIG_HOME") or (os.getenv("HOME") .. "/.config")) .. "/hypr/workspace-ribbon.lua")
```

Run `hyprctl reload` and check `hyprctl configerrors` after adding the line. Future saves use the same file.
