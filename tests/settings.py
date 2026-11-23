"""Exercise panel live settings without changing the user's Hyprland session."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

HELPER = Path(__file__).resolve().parents[1] / "bin" / "ribbon-settings"
DEFAULTS = dict(enabled=1, zoom_out_percent=8, duration_ms=400,
                easing=1, scale_distance=0, max_duration_ms=1200)
BOUNDS = dict(enabled=(0, 1), zoom_out_percent=(0, 25), duration_ms=(1, 10000),
              easing=(0, 2), scale_distance=(0, 1), max_duration_ms=(1, 10000))

FAKE_HYPRCTL = r"""#!/usr/bin/env python3
import json
import os
from pathlib import Path
import re
import sys

args = sys.argv[1:]
state = Path(os.environ["RIBBON_TEST_STATE"])
if os.environ.get("RIBBON_TEST_OFFLINE"):
    print("Couldn't connect to the Wayland socket")
elif args == ["-j", "plugin", "list"]:
    print(json.dumps([{"name":"workspace-ribbon", "handle":"fixture"}] if state.exists() else []))
elif args[:2] == ["-j", "--batch"]:
    if state.exists():
        for name, value in json.loads(state.read_text()).items():
            print(json.dumps({"option": "plugin:workspace_ribbon:" + name, "int": value}))
    else:
        print("no such option")
elif args[0] == "eval":
    key, value = re.search(r"workspace_ribbon=\{(\w+)=(\d+)", args[1]).groups()
    values = json.loads(state.read_text())
    values[key] = int(value)
    state.write_text(json.dumps(values))
    print("ok")
else:
    raise SystemExit(2)
"""


class PanelSettings(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)
        self.hypr = self.folder / "hypr"
        self.hypr.mkdir()
        self.state = self.folder / "active.json"
        self.state.write_text(json.dumps(DEFAULTS))
        fake = self.folder / "hyprctl"
        fake.write_text(FAKE_HYPRCTL)
        fake.chmod(0o700)
        self.env = dict(os.environ, PATH=str(self.folder) + ":" + os.environ["PATH"],
                        XDG_CONFIG_HOME=str(self.folder), XDG_DATA_HOME=str(self.folder / "data"),
                        XDG_CACHE_HOME=str(self.folder / "cache"), XDG_RUNTIME_DIR=str(self.folder),
                        RIBBON_TEST_STATE=str(self.state), HYPRLAND_INSTANCE_SIGNATURE="fixture")

    def command(self, *args, ok=True):
        process = subprocess.run([HELPER, *args], env=self.env, cwd=self.folder,
                                 capture_output=True, text=True)
        reply = json.loads(process.stdout)
        self.assertEqual(ok, reply["ok"], reply)
        self.assertEqual(ok, process.returncode == 0, reply)
        return reply

    def test_live_options_at_bounds(self):
        first = self.command("get")
        self.assertEqual(first["values"], DEFAULTS)
        self.assertFalse(first["saved"])
        self.assertEqual(first["login"], "external")
        for key, (low, high) in BOUNDS.items():
            for edge in (low, high):
                self.assertEqual(self.command("set", key, str(edge))["values"][key], edge)
            self.command("set", key, str(low - 1), ok=False)
            self.command("set", key, str(high + 1), ok=False)
            self.assertEqual(self.command("get")["values"][key], high)

    def test_atomic_save_and_stale_snapshot(self):
        path = self.hypr / "workspace-ribbon.lua"
        self.command("set", "zoom_out_percent", "12")
        self.command("save", "--expect", json.dumps(DEFAULTS), ok=False)
        self.assertFalse(path.exists())
        self.assertTrue(self.command("save")["saved"])
        original_inode = path.stat().st_ino
        self.assertIn("zoom_out_percent = 12,", path.read_text())
        self.assertFalse(self.command("set", "zoom_out_percent", "8")["saved"])
        self.assertTrue(self.command("save")["saved"])
        self.assertNotEqual(original_inode, path.stat().st_ino)
        self.assertEqual(list(self.hypr.iterdir()), [path])

    def test_native_absent_offline_and_existing_loader(self):
        self.assertIn("already loaded", self.command("build", ok=False)["message"])
        self.state.unlink()
        self.assertEqual(self.command("get", ok=False)["code"], "not_loaded")
        loader = self.hypr / "autostart.lua"
        contents = 'hl.plugin.load("/example/workspace-ribbon/libworkspace-ribbon.so")\n'
        loader.write_text(contents)
        self.assertIn("login loader is already configured", self.command("build", ok=False)["message"])
        self.assertEqual(loader.read_text(), contents)
        self.env["RIBBON_TEST_OFFLINE"] = "1"
        self.assertEqual(self.command("get", ok=False)["code"], "unavailable")


if __name__ == "__main__":
    unittest.main()
