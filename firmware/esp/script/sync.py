
Import("env")  # type: ignore[name-defined]

import shutil
from pathlib import Path

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]

WEB_SRC_DIR = PROJECT_DIR.parent.parent / "website" / "src"
DATA_DIR = PROJECT_DIR / "data"

print("Syncing web source files to data directory...")

if DATA_DIR.exists():
    shutil.rmtree(DATA_DIR)

shutil.copytree(WEB_SRC_DIR, DATA_DIR)

print("Sync complete.")