#!/usr/bin/env python3
"""Stage the firmware images and ESP Web Tools manifests for the Web Flasher.

Copies bootloader, partition table, boot_app0 and application image for each
board variant into webflasher/firmware/<env>/ and writes a manifest.json next
to them, so webflasher/ can be served as-is (GitHub Pages or any HTTPS host).

    python tools/build_webflasher.py            # use existing .pio/build output
    python tools/build_webflasher.py --build    # run `pio run` for each env first
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "webflasher" / "firmware"

# PlatformIO env -> label shown by the flasher. Keep in sync with platformio.ini
# and the board list in webflasher/index.html.
ENVS = {
    "esp32-2432s028r": 'CYD Ham Dashboard - 2.8" ILI9341',
    "esp32-2432s028r-st7789": 'CYD Ham Dashboard - 2.8" ST7789',
    "esp32-4in-st7796": 'CYD Ham Dashboard - 4.0" ST7796',
}

# ESP32 Arduino flash layout (min_spiffs.csv keeps otadata at 0xe000).
BOOTLOADER_OFFSET = 0x1000
PARTITIONS_OFFSET = 0x8000
BOOT_APP0_OFFSET = 0xE000
APP_OFFSET = 0x10000


def git_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=ROOT, text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "dev"


def find_boot_app0():
    core_dir = Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio"))
    path = core_dir / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"
    if not path.is_file():
        sys.exit(f"boot_app0.bin not found at {path}; build once with PlatformIO first")
    return path


def stage_env(env, label, version, boot_app0):
    build = ROOT / ".pio" / "build" / env
    parts = [
        ("bootloader.bin", build / "bootloader.bin", BOOTLOADER_OFFSET),
        ("partitions.bin", build / "partitions.bin", PARTITIONS_OFFSET),
        ("boot_app0.bin", boot_app0, BOOT_APP0_OFFSET),
        ("firmware.bin", build / "firmware.bin", APP_OFFSET),
    ]
    missing = [str(src) for _, src, _ in parts if not src.is_file()]
    if missing:
        sys.exit(f"{env}: missing build output {', '.join(missing)} (run with --build)")

    dest = OUT / env
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    for name, src, _ in parts:
        shutil.copyfile(src, dest / name)

    manifest = {
        "name": label,
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{
            "chipFamily": "ESP32",
            "parts": [{"path": name, "offset": offset} for name, _, offset in parts],
        }],
    }
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"{env}: {version} -> {dest.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", action="store_true", help="run `pio run` for each env first")
    parser.add_argument("--version", default=None, help="version string (default: git describe)")
    parser.add_argument("--allow-local-config", action="store_true",
                        help="stage images even though include/app_config.local.h exists")
    parser.add_argument("envs", nargs="*", default=list(ENVS), help="envs to stage (default: all)")
    args = parser.parse_args()

    unknown = [e for e in args.envs if e not in ENVS]
    if unknown:
        sys.exit(f"unknown env(s): {', '.join(unknown)}")

    # The local override bakes Wi-Fi credentials and API keys into the image.
    local_config = ROOT / "include" / "app_config.local.h"
    if local_config.exists() and not args.allow_local_config:
        sys.exit(f"{local_config.relative_to(ROOT)} exists: its Wi-Fi password and API keys "
                 "would be compiled into public images. Move it aside and rebuild, "
                 "or pass --allow-local-config for a private test.")

    if args.build:
        cmd = ["pio", "run"]
        for env in args.envs:
            cmd += ["-e", env]
        subprocess.check_call(cmd, cwd=ROOT)

    version = args.version or git_version()
    boot_app0 = find_boot_app0()
    for env in args.envs:
        stage_env(env, ENVS[env], version, boot_app0)


if __name__ == "__main__":
    main()
