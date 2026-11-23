"""User-local lifecycle management for the Workspace Ribbon native plugin."""
import contextlib
import fcntl
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess
import tempfile
import tomllib


ROOT = Path(__file__).resolve().parent.parent
NAME = "workspace-ribbon"
BEGIN = "-- BEGIN Workspace Ribbon managed login"
END = "-- END Workspace Ribbon managed login"
MARKER = re.compile(r"(?m)^" + BEGIN + r"\n(.*?)^" + END + r"\n?", re.S | re.M)
# An installer can only identify explicit loaders, not arbitrary Lua logic.
LOADER = re.compile(
    r"(?:plugin\s*\.\s*load|hyprctl\s+plugin\s+load|hyprpm\s+enable)"
    r"[^\n]*(?:workspace[-_]ribbon|libworkspace-ribbon)", re.I
)


class SettingsError(Exception):
    def __init__(self, message, code="error"):
        super().__init__(message)
        self.code = code


def xdg(variable, fallback):
    return Path(os.environ.get(variable) or Path.home() / fallback).expanduser().resolve()


def config_dir():
    return xdg("XDG_CONFIG_HOME", ".config") / "hypr"


def data_dir():
    return xdg("XDG_DATA_HOME", ".local/share") / NAME


def cache_dir():
    return xdg("XDG_CACHE_HOME", ".cache") / NAME


def atomic_write(path, content, mode=0o600):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix="." + path.name + "-", delete=False) as file:
        temporary = Path(file.name)
        try:
            file.write(content)
            file.flush()
            os.fsync(file.fileno())
            temporary.chmod(mode)
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def read_text(path):
    try:
        return path.read_bytes().decode("utf-8")
    except FileNotFoundError:
        return ""


def active_plugin(ctl):
    plugins = json.loads(ctl("-j", "plugin", "list"))
    return next((item for item in plugins if item.get("name") == NAME), None)


def session_receipt():
    signature = os.environ.get("HYPRLAND_INSTANCE_SIGNATURE")
    if not signature:
        raise SettingsError("No Hyprland session signature is available.", "unavailable")
    key = hashlib.sha256(signature.encode()).hexdigest()
    return xdg("XDG_RUNTIME_DIR", ".cache") / NAME / (key + ".json")


def owned_path(plugin):
    if not plugin or not os.environ.get("HYPRLAND_INSTANCE_SIGNATURE"):
        return None
    try:
        receipt = json.loads(read_text(session_receipt()))
        path = Path(receipt["path"])
        if receipt["handle"] == plugin["handle"] and path.is_relative_to(data_dir()) and path.is_file():
            return path
    except (ValueError, KeyError, OSError, TypeError):
        pass
    return None


def configured_loader():
    """Return the first explicit external loader without changing configuration."""
    directory = config_dir()
    for path in sorted(directory.rglob("*")):
        if path.suffix not in {".lua", ".conf"} or not path.is_file():
            continue
        source = read_text(path)
        if path == directory / "autostart.lua":
            source = MARKER.sub("", source)
        for line in source.splitlines():
            if not line.lstrip().startswith(("--", "#")) and LOADER.search(line):
                return str(path)
    username = getpass.getuser()
    stores = (Path("/var/cache/hyprpm") / username, xdg("XDG_DATA_HOME", ".local/share") / "hyprpm")
    for store in stores:
        for state in store.glob("*/state.toml"):
            try:
                data = tomllib.loads(read_text(state))
            except tomllib.TOMLDecodeError as exc:
                if NAME in read_text(state):
                    raise SettingsError(f"Cannot verify the Workspace Ribbon entry in {state}.") from exc
                continue
            entry = data.get(NAME)
            if isinstance(entry, dict) and entry.get("enabled") is True:
                return f"hyprpm ({state})"
    return None


def managed_block():
    path = config_dir() / "autostart.lua"
    return path, read_text(path)


def login_status(plugin):
    _, source = managed_block()
    conflict = configured_loader()
    if MARKER.search(source):
        return "managed", conflict or ""
    if conflict:
        return "external", conflict
    if plugin and not owned_path(plugin):
        return "external", "An existing session load"
    return "available", ""


@contextlib.contextmanager
def native_lock():
    cache_dir().mkdir(parents=True, exist_ok=True)
    with (cache_dir() / "native.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise SettingsError("Another Workspace Ribbon setup operation is running. Retry in a moment.") from exc
        yield


def checked_tool(command, label):
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise SettingsError(f"{label}: {exc}") from exc
    if result.returncode:
        reason = (result.stderr.strip() or result.stdout.strip()).splitlines()
        raise SettingsError(f"{label}: {reason[0] if reason else 'command failed'}")
    return result.stdout.strip()


def build_preflight(ctl):
    version = json.loads(ctl("-j", "version"))
    build_source = (ROOT / "CMakeLists.txt").read_text()
    supported = re.search(r"\bhyprland=([\d.]+)\b", build_source).group(1)
    if version.get("version") != supported:
        raise SettingsError(f"The running Hyprland is {version.get('version', 'unknown')}. This checkout requires {supported}.")

    cmake = shutil.which("cmake")
    if not cmake:
        # Support an existing project-local virtual environment as well as PATH.
        local_cmake = ROOT / ".tools" / "bin" / "cmake"
        if not os.access(local_cmake, os.X_OK):
            raise SettingsError("CMake 3.27 or later is required. Install cmake and retry.")
        cmake = str(local_cmake)
    for command in ("pkg-config", "c++", "make"):
        if not shutil.which(command):
            raise SettingsError(f"Missing build dependency: {command}. Install it and retry.")

    installed = checked_tool(["pkg-config", "--modversion", "hyprland"], "Hyprland development headers are missing")
    if installed != supported:
        raise SettingsError(f"Installed Hyprland headers are {installed}; the running session requires {supported}.")
    flags = checked_tool(["pkg-config", "--cflags-only-I", "hyprland"], "Cannot locate Hyprland headers")
    roots = [Path(flag[2:]) for flag in shlex.split(flags) if flag.startswith("-I") and len(flag) > 2]
    source_dirs = (root / suffix for root in roots for suffix in ("hyprland/src", "src", "."))
    headers = next((directory for directory in source_dirs if (directory / "plugins/PluginAPI.hpp").is_file()
                    and (directory / "version.h").is_file()), None)
    if headers is None:
        raise SettingsError("Cannot locate the Hyprland plugin and ABI headers in pkg-config's include paths.")
    header = headers / "version.h"
    fields = dict(re.findall(r'^#define\s+(\w+)\s+"([^"]+)"', header.read_text(), re.M))
    libraries = (("aq", "AQUAMARINE"), ("hu", "HYPRUTILS"), ("hg", "HYPRGRAPHICS"),
                 ("hc", "HYPRCURSOR"), ("hlg", "HYPRLANG"))
    abi = fields.get("GIT_COMMIT_HASH", "") + "".join(
        "_" + short + "_" + fields.get(name + "_VERSION", "").rsplit(".", 1)[0]
        for short, name in libraries
    )
    if abi != version.get("abiHash"):
        raise SettingsError("The installed Hyprland headers and running compositor have different ABIs. Use matching headers and session.")
    return cmake, abi


def compile_native(ctl):
    cmake, abi = build_preflight(ctl)
    log = cache_dir() / "build.log"
    with tempfile.TemporaryDirectory(dir=cache_dir(), prefix="build-") as folder, log.open("w+") as output:
        build = Path(folder)
        commands = [
            ("CMake configuration", [cmake, "-S", str(ROOT), "-B", str(build), "-G", "Unix Makefiles",
                                     "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF"]),
            ("Native build", [cmake, "--build", str(build), "--parallel", "2"]),
        ]
        for phase, command in commands:
            try:
                result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=600)
            except subprocess.TimeoutExpired as exc:
                raise SettingsError(f"{phase} timed out. Build log: {log}") from exc
            if result.returncode:
                output.seek(0)
                lines = output.read().splitlines()
                errors = [line.strip() for line in lines if re.search(r"error:|not found|required package", line, re.I)]
                detail = " ".join(errors[:2] or lines[-3:])[:230]
                raise SettingsError(f"{phase} failed. {detail} Build log: {log}")

        contents = (build / "libworkspace-ribbon.so").read_bytes()
        fingerprint = hashlib.sha256(contents).hexdigest()[:20]
        path = data_dir() / abi / fingerprint / "libworkspace-ribbon.so"
        if not path.is_file():
            atomic_write(path, contents, 0o755)
        return path


def load_conflict(ctl):
    if active_plugin(ctl):
        return "Workspace Ribbon is already loaded. Keep using its settings, or prepare an update without loading it."
    _, source = managed_block()
    conflict = configured_loader()
    if conflict or MARKER.search(source):
        return f"A login loader is already configured ({conflict or 'managed autostart.lua'}). Resolve it before a session-only load."
    return None


def build_and_enable(ctl, read_active, prepare):
    with native_lock():
        if not prepare and (reason := load_conflict(ctl)):
            raise SettingsError(reason)
        path = compile_native(ctl)
        if prepare:
            return path, None
        # Recheck after compilation in case another loader became active.
        if reason := load_conflict(ctl):
            raise SettingsError(f"{reason} The new binary is available at {path}.")
        receipt = session_receipt()
        answer = ctl("plugin", "load", str(path))
        if answer != "ok":
            raise SettingsError(f"Hyprland could not load Workspace Ribbon: {answer[:320]}")
        # This call loaded the exact path. Keep the receipt even if the option
        # check fails, so the managed binary can still be removed.
        plugin = active_plugin(ctl)
        if not plugin:
            raise SettingsError("Hyprland returned success, but Workspace Ribbon is missing from the plugin list.")
        atomic_write(receipt, json.dumps({"handle": plugin["handle"], "path": str(path)}).encode())
        return path, read_active()


def set_login(ctl, enable):
    with native_lock():
        path, source = managed_block()
        markers = (source.count(BEGIN), source.count(END))
        blocks = list(MARKER.finditer(source))
        if markers not in ((0, 0), (1, 1)) or len(blocks) != markers[0]:
            raise SettingsError(f"Review the Workspace Ribbon login markers in {path}.")
        if enable:
            if conflict := configured_loader():
                raise SettingsError(f"Workspace Ribbon already has another loader: {conflict}")
            if blocks:
                return
            plugin = active_plugin(ctl)
            library = owned_path(plugin)
            if not library:
                raise SettingsError("Login loading can be enabled here only for a build activated by this panel.")
            root = read_text(config_dir() / "hyprland.lua")
            if not re.search(r"""(?m)^\s*require\(\s*['"]hypr\.autostart['"]\s*\)""", root):
                raise SettingsError("This Hyprland config does not load the standard hypr.autostart module. Configure the loader manually.")
            literal = json.dumps(str(library), ensure_ascii=False)
            updated = source + ("" if not source or source.endswith("\n") else "\n")
            updated += f"{BEGIN}\nhl.plugin.load({literal})\n{END}\n"
        elif blocks:
            updated = MARKER.sub("", source)
        else:
            return
        if path.is_symlink():
            raise SettingsError(f"{path} is a symlink. Edit its managed login block manually.")
        if read_text(path) != source:
            raise SettingsError(f"{path} changed during this operation. Retry.")
        mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o600
        atomic_write(path, updated.encode(), mode)


def unload_owned(ctl):
    with native_lock():
        if MARKER.search(managed_block()[1]):
            raise SettingsError("Turn off Load at login before unloading the native plugin.")
        path = owned_path(active_plugin(ctl))
        if not path:
            raise SettingsError("Use the original loader to unload this Workspace Ribbon instance.")
        answer = ctl("plugin", "unload", str(path))
        if answer != "ok" or active_plugin(ctl):
            raise SettingsError(f"Hyprland did not unload Workspace Ribbon: {answer[:320]}")
        session_receipt().unlink(missing_ok=True)
