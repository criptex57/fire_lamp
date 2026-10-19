"""Embed the offline UI in flash; no filesystem upload needed."""
from pathlib import Path
Import("env")
root = Path(env["PROJECT_DIR"])
page = (root / "web/index.html").read_text()
assert ')LAMPHTML"' not in page
(root / "include/web_ui.generated.h").write_text(
    '#pragma once\n#include <Arduino.h>\nconst char WEB_UI[] PROGMEM = R"LAMPHTML('
    + page + ')LAMPHTML";\n'
)
