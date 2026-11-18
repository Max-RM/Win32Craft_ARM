#!/usr/bin/env python3
"""Open-Saves-Folder helper for the Win32Craft packages.

Every device package ships two files next to the game:

  Open-Saves-Folder.bat   opens the folder the app saves into (creates it if needed)
  Open-Saves-Folder.lnk   double-clickable shortcut that does the same, without the console

Why a .bat instead of a direct folder shortcut: the save folder lives under the user's
%APPDATA%, which is only known on the device.  The .bat expands it at run time, and the .lnk
runs cmd.exe with the same command, so neither file contains a build-machine path and both
survive copying the package to another machine.

Data roots of the ports (keep in sync with the host profiles):

  Minecraft 1.2.8 ARM32            %APPDATA%\\MinecraftPE
  Minecraft PE 0.10.4.40           %APPDATA%\\MinecraftPE\\0.10.4
  REDCON 1 1.3.0.0                 %APPDATA%\\Win32Craft\\Redcon
  Hill Climb Racing 1.27.0.32      %APPDATA%\\Win32Craft\\HillClimbRacing

Usage (from a packaging script):

    from saves_folder import write_saves_helper
    added = write_saves_helper(out_folder, "Win32Craft\\\\HillClimbRacing", "Hill Climb Racing")

The function never raises for a missing PowerShell / non-Windows build: it writes the .bat
anyway and prints a note about the missing .lnk.
"""
import os
import subprocess
import sys
from pathlib import Path

BAT_NAME = "Open-Saves-Folder.bat"
LNK_NAME = "Open-Saves-Folder.lnk"

BAT = """@echo off
rem Win32Craft - open the folder where this app keeps its saves.
rem {app}
rem The game creates the folder on its first start; this creates it too if it is still missing.
setlocal
set "SAVES=%APPDATA%\\{subdir}"
if not exist "%SAVES%" mkdir "%SAVES%"
start "" "%SAVES%"
"""

# cmd.exe is on every supported device (Windows RT 8.1, Windows 10/11 ARM32/ARM64, Wine); cmd
# expands %APPDATA% when the shortcut runs, so the stored command is portable.
LNK_TARGET = "%windir%\\system32\\cmd.exe"
LNK_ARGUMENTS = ('/c if not exist "%APPDATA%\\{subdir}" mkdir "%APPDATA%\\{subdir}" & '
                 'start "" "%APPDATA%\\{subdir}"')
LNK_DESCRIPTION = "Open the {app} save folder"


def _check_subdir(data_subdir: str) -> str:
    subdir = data_subdir.strip().strip("\\/")
    if not subdir or subdir.startswith((":", "/")) or ".." in subdir or '"' in subdir:
        raise ValueError("refusing an unsafe save folder: %r" % data_subdir)
    return subdir


def _ps_quote(text: str) -> str:
    return "'" + text.replace("'", "''") + "'"


def _write_shortcut(lnk: Path, subdir: str, app: str) -> None:
    script = ";".join([
        "$ErrorActionPreference='Stop'",
        "$s=(New-Object -ComObject WScript.Shell).CreateShortcut(%s)" % _ps_quote(str(lnk)),
        "$s.TargetPath=%s" % _ps_quote(LNK_TARGET),
        "$s.Arguments=%s" % _ps_quote(LNK_ARGUMENTS.format(subdir=subdir)),
        "$s.WindowStyle=7",
        "$s.Description=%s" % _ps_quote(LNK_DESCRIPTION.format(app=app)),
        "$s.IconLocation='%SystemRoot%\\explorer.exe,0'",
        "$s.Save()",
    ])
    result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
                            capture_output=True, text=True)
    if result.returncode != 0 or not lnk.is_file():
        raise RuntimeError((result.stderr or result.stdout or "powershell failed").strip())


def write_saves_helper(target: Path, data_subdir: str, app: str) -> list[str]:
    """Write Open-Saves-Folder.bat (+ .lnk on Windows) into `target`; returns the names created."""
    subdir = _check_subdir(data_subdir)
    target = Path(target)
    target.mkdir(parents=True, exist_ok=True)

    bat = target / BAT_NAME
    bat.write_bytes(BAT.format(app=app, subdir=subdir).replace("\n", "\r\n").encode("ascii", "replace"))
    created = [BAT_NAME]

    lnk = target / LNK_NAME
    if os.name != "nt":
        print("note: %s not created (needs Windows/PowerShell); %s is included" % (LNK_NAME, BAT_NAME))
        return created
    try:
        _write_shortcut(lnk, subdir, app)
    except Exception as error:  # noqa: BLE001 - packaging must not fail because of a shortcut
        print("note: %s not created (%s); %s is included" % (LNK_NAME, error, BAT_NAME),
              file=sys.stderr)
        return created
    created.append(LNK_NAME)
    return created
