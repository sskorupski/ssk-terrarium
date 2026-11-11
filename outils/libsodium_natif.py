# PlatformIO pre-script of the native environment: links libsodium, found by
# pkg-config, so that the host tests verify minisign signatures with the same
# primitives as the board (RG-image-signee). On the ESP32, libsodium comes
# with the Arduino SDK. Install it with `brew install libsodium pkgconf`
# (macOS) or `apt-get install libsodium-dev pkg-config` (Linux).
import subprocess
import sys

Import("env")  # noqa: F821 (provided by PlatformIO)

try:
    flags = subprocess.run(
        ["pkg-config", "--cflags", "--libs", "libsodium"],
        capture_output=True, text=True, check=True,
    ).stdout.strip()
except (OSError, subprocess.CalledProcessError):
    sys.stderr.write(
        "libsodium introuvable par pkg-config : brew install libsodium pkgconf, "
        "ou apt-get install libsodium-dev pkg-config\n"
    )
    env.Exit(1)  # noqa: F821

env.MergeFlags(flags)  # noqa: F821
