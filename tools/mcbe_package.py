#!/usr/bin/env python3
"""Shared package conventions for the MCPE / MCBE (Minecraft Bedrock) ports.

Every Win32Craft package of a Minecraft Bedrock build carries a ``.Win32Craft``
file in its root.  The file contains the game type as plain ASCII text:

    Minecraft Bedrock Edition

External repack tools can use it to tell a Win32Craft folder from a stock UWP
or APPX folder without guessing from the executable name.  Signed package
identity files (``*.p7x``) are meaningless outside the package graph and are
removed from the output folder.

Only the Minecraft Bedrock scripts use this module; the other Win32Craft
games keep their existing package layout.
"""

from __future__ import annotations

from pathlib import Path

GAME_TYPE = "Minecraft Bedrock Edition"
MARKER_NAME = ".Win32Craft"


def remove_p7x_files(folder: Path) -> list[Path]:
    """Delete every ``*.p7x`` file below *folder* and return the removed paths."""
    removed: list[Path] = []
    for path in sorted(folder.rglob("*")):
        if path.is_file() and path.suffix.lower() == ".p7x":
            path.unlink()
            removed.append(path)
    return removed


def write_game_marker(folder: Path, game_type: str = GAME_TYPE) -> Path:
    """Write the ``.Win32Craft`` marker and return its path.

    The marker is written without a trailing newline so that a simple
    ``read_text().strip()`` and an exact string comparison both work.
    """
    marker = folder / MARKER_NAME
    marker.write_text(game_type, encoding="ascii")
    return marker


def prepare_mcbe_package(folder: Path) -> Path:
    """Apply the MCPE/MCBE package marker and remove ``*.p7x`` files.

    Returns the marker path.  Safe to call more than once.
    """
    removed = remove_p7x_files(folder)
    for path in removed:
        print(f"removed {path.relative_to(folder)}")
    marker = write_game_marker(folder)
    print(f"wrote {MARKER_NAME} ({GAME_TYPE})")
    return marker
