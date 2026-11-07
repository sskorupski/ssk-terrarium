# PlatformIO post-script: stamps the firmware with its Git version, printed in
# the boot banner. On a release it is the tag itself (phase-1); in between, the
# last tag, the commits since and the commit hash (phase-1-3-g1a2b3c4).
# Applied to the project sources only, so a new commit does not rebuild the
# framework and the libraries.
import subprocess

Import("projenv")  # noqa: F821 (provided by PlatformIO)

try:
    version = subprocess.run(
        ["git", "describe", "--tags", "--always", "--dirty"],
        capture_output=True, text=True, check=True,
        cwd=projenv.subst("$PROJECT_DIR"),  # noqa: F821
    ).stdout.strip()
except (OSError, subprocess.CalledProcessError):
    version = "inconnue"

projenv.Append(CPPDEFINES=[("FIRMWARE_VERSION", projenv.StringifyMacro(version))])  # noqa: F821
