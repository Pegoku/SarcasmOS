"""PlatformIO pre-build hook for the generated mouth-asset header."""

from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.

project_dir = Path(env["PROJECT_DIR"])  # type: ignore[name-defined]
source = project_dir / "assets" / "mouth_assets.json"
target = project_dir / "generated" / "mouth_assets.hpp"
font_source = project_dir / "assets" / "temperature_font.json"
font_target = project_dir / "generated" / "temperature_font.hpp"
tool = project_dir / "asset_tool.py"

if (not target.exists() or target.stat().st_mtime < source.stat().st_mtime or
        target.stat().st_mtime < tool.stat().st_mtime):
    import sys

    sys.path.insert(0, str(project_dir))
    from asset_tool import compile_assets

    compile_assets(source, target)

if (not font_target.exists() or
        font_target.stat().st_mtime < font_source.stat().st_mtime or
        font_target.stat().st_mtime < tool.stat().st_mtime):
    import sys

    sys.path.insert(0, str(project_dir))
    from asset_tool import compile_temperature_font

    compile_temperature_font(font_source, font_target)
