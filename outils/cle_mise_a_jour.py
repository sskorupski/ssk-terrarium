# PlatformIO post-script: compiles the owner's minisign public key into the
# firmware (RG-image-signee). The key is read from outils/mise-a-jour.pub, the
# file `minisign -G` writes, made by outils/cle-mise-a-jour.sh. Without it, no
# firmware: a board must never accept an image the owner has not signed.
# Applied to the project sources only, like the version.
import os
import re

Import("projenv")  # noqa: F821 (provided by PlatformIO)

path = os.path.join(projenv.subst("$PROJECT_DIR"), "outils", "mise-a-jour.pub")  # noqa: F821
try:
    with open(path, encoding="ascii") as file:
        lines = file.read().splitlines()
except OSError:
    lines = []

# A minisign public key: "Ed", the key id and the Ed25519 key, in base64.
if len(lines) < 2 or not re.fullmatch(r"RW[A-Za-z0-9+/]{54}", lines[1]):
    raise SystemExit(
        "outils/mise-a-jour.pub absente ou illisible : la générer par "
        "outils/cle-mise-a-jour.sh (docs/pratiques.md, « Publier une mise à jour »)"
    )

projenv.Append(CPPDEFINES=[("UPDATE_PUBLIC_KEY", projenv.StringifyMacro(lines[1]))])  # noqa: F821
