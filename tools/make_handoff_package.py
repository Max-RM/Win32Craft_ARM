"""Assemble the Win32Craft hand-off bundle to send to the friend (runnable games + source + the hand-off doc).

Layout produced under dist-handoff/Win32Craft_Handoff/:
  HANDOFF_FOR_FRIEND.md          the hand-off document (must already exist at the repo root)
  README.txt                     one-screen index of the bundle
  runnable/                       the three built, device-runnable game packages (contain extracted game DLLs/assets)
    REDCON_1.3.0.0_ARM32_Win32Craft.zip
    BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip
    GeometryDash_M1_host.zip
    gd_frame.png                 proof: Geometry Dash's splash rendered through the host at FL 9_1
  source.zip                     the Win32Craft SOURCE + tooling to continue (NO game folders / appx / xap / pdf)

Source is an ALLOWLIST so no copyrighted game content can leak. Run: python -I tools/make_handoff_package.py
"""
import os, shutil, zipfile, sys, fnmatch

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "dist-handoff", "Win32Craft_Handoff")
DOC = os.path.join(ROOT, "HANDOFF_FOR_FRIEND.md")

# runnable packages: (source path, name inside runnable/)
RUNNABLE = [
    (os.path.join(ROOT, "dist-redcon", "REDCON_1.3.0.0_ARM32_Win32Craft.zip"), "REDCON_1.3.0.0_ARM32_Win32Craft.zip"),
    (os.path.join(ROOT, "dist-badpiggies", "BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip"), "BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip"),
    (os.path.join(ROOT, "dist-gd-host.zip"), "GeometryDash_1.0.0.0_ARM32_Win32Craft.zip"),
    (os.path.join(ROOT, "dist-gd", "gd_frame_900.png"), "gd_gameplay.png"),
]

# extra per-game continuation docs copied to the bundle root next to HANDOFF_FOR_FRIEND.md
EXTRA_DOCS = [
    os.path.join(ROOT, "games_recon", "HANDOFF_GD_IMPROVEMENTS.md"),
    os.path.join(ROOT, "games_recon", "HANDOFF_BADPIGGIES_MODS.md"),
    os.path.join(ROOT, "games_recon", "HANDOFF_FOR_BADPIGGIES_DEV.md"),
]

# ---- source bundle policy (allowlist) ----
# whole directories to include (recursively), minus the skip rules below
DIRS_FULL = ["tools", "games_recon", "wp81"]
# arm32/ holds valuable RE notes but also scratch binaries; include TEXT/source files only
DIR_TEXT_ONLY = {"arm32": (".md", ".txt", ".log", ".json", ".csv", ".py", ".c", ".h", ".inl", ".bat", ".sh", ".def")}
# individual top-level files to include
FILES_TOP = (
    ["win32_host.c", "win32_host_116.inl", "win32_host_early2016.c", "api_compat.c", "win32_compat.h",
     "win32craft.def", "win32craft_arm32.def",
     "api-ms-win-core-winrt-error-l1-1-0.def", "api-ms-win-core-winrt-l1-1-0.def", "api-ms-win-core-winrt-string-l1-1-0.def",
     "ARM32.md", "HANDOFF_ARM32.md", "LICENSE", "HANDOFF_FOR_FRIEND.md"]
)
FILES_TOP_GLOB = ["win32_host_arm32*.inl", "win32_host_arm32*.h", "build_*.bat", "build_*.sh"]
# never copy these anywhere in the source bundle
SKIP_ANY = ["__pycache__", ".git", ".cache"]
SKIP_EXT = (".appx", ".xap", ".pdf", ".pyc")

def skip(path):
    low = path.replace("\\", "/").lower()
    if any(("/" + s + "/") in ("/" + low + "/") or low.endswith("/" + s) or ("/" + s) in low for s in SKIP_ANY):
        return True
    return low.endswith(SKIP_EXT)

def add_tree(z, src_root, arc_root, text_exts=None):
    for dp, dns, fns in os.walk(src_root):
        dns[:] = [d for d in dns if d not in SKIP_ANY]
        for fn in fns:
            full = os.path.join(dp, fn)
            if skip(full):
                continue
            if text_exts is not None and not fn.lower().endswith(text_exts):
                continue
            rel = os.path.relpath(full, ROOT)
            z.write(full, os.path.join(arc_root, rel))

def build_source_zip(dest):
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for d in DIRS_FULL:
            p = os.path.join(ROOT, d)
            if os.path.isdir(p):
                add_tree(z, p, "Win32Craft_src")
        for d, exts in DIR_TEXT_ONLY.items():
            p = os.path.join(ROOT, d)
            if os.path.isdir(p):
                add_tree(z, p, "Win32Craft_src", text_exts=exts)
        names = list(FILES_TOP)
        for g in FILES_TOP_GLOB:
            names += [f for f in os.listdir(ROOT) if fnmatch.fnmatch(f, g)]
        for fn in sorted(set(names)):
            full = os.path.join(ROOT, fn)
            if os.path.isfile(full) and not skip(full):
                z.write(full, os.path.join("Win32Craft_src", fn))
    # safety: assert no game content leaked - flag *.appx/.xap/.pdf anywhere, or a GAME FOLDER appearing as a
    # directory segment (i.e. a file copied from inside a game folder). A file merely NAMED after a game (e.g. an
    # arm32/names/*.jsonl symbol map) is RE output, not game content, and is kept.
    GAME_DIRS = {"redcon_1.3.0.0_arm", "geometry_dash_2.0.1.0", "badpiggiesrebooted_1.0.0.0_arm",
                 "minecraft_1.16.40.02", "minecraft_1.2.8.0_arm32", "whatsapp.qt_0.2.0.8_arm"}
    with zipfile.ZipFile(dest) as z:
        bad = []
        for n in z.namelist():
            low = n.replace("\\", "/").lower()
            segs = low.split("/")
            if low.endswith((".appx", ".xap", ".pdf")) or (set(segs[:-1]) & GAME_DIRS):
                bad.append(n)
        if bad:
            print("ERROR: game content leaked into source.zip:", bad[:5]); sys.exit(9)
        return len(z.namelist())

def main():
    if not os.path.exists(DOC):
        print("ERROR: write HANDOFF_FOR_FRIEND.md at the repo root first."); return 2
    for src, _ in RUNNABLE:
        if not os.path.exists(src):
            print("ERROR: missing runnable artifact:", src); return 3

    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(os.path.join(OUT, "runnable"))

    shutil.copyfile(DOC, os.path.join(OUT, "HANDOFF_FOR_FRIEND.md"))
    for d in EXTRA_DOCS:
        if os.path.exists(d):
            shutil.copyfile(d, os.path.join(OUT, os.path.basename(d)))
        else:
            print("WARNING: missing extra doc:", d)
    for src, name in RUNNABLE:
        shutil.copyfile(src, os.path.join(OUT, "runnable", name))

    nsrc = build_source_zip(os.path.join(OUT, "source.zip"))

    with open(os.path.join(OUT, "README.txt"), "w", newline="\r\n") as f:
        f.write("Win32Craft - hand-off bundle\r\n")
        f.write("============================\r\n\r\n")
        f.write("Running UWP / Windows Phone ARM32 games as native Win32 on ARM Windows.\r\n\r\n")
        f.write("START HERE: open HANDOFF_FOR_FRIEND.md (full status, how to run each game, how it works, how to continue).\r\n")
        f.write("Per-game continuation docs: HANDOFF_GD_IMPROVEMENTS.md (Geometry Dash), HANDOFF_BADPIGGIES_MODS.md (Bad\r\n")
        f.write("Piggies), HANDOFF_FOR_BADPIGGIES_DEV.md (a bug report for the BP mod author).\r\n\r\n")
        f.write("runnable/ - the three built packages (copy to an ARM Windows device, Developer Mode on):\r\n")
        f.write("  REDCON_1.3.0.0_ARM32_Win32Craft.zip           Redcon - runs (sound+music, ~30 fps)\r\n")
        f.write("  BadPiggiesRebooted_1.0.0.0_ARM_Win32Craft.zip Bad Piggies - renders+runs at D3D11 FL 9_1\r\n")
        f.write("  GeometryDash_1.0.0.0_ARM32_Win32Craft.zip     Geometry Dash - PLAYS at FL 9_1 (extract to a NON-root\r\n")
        f.write("                                                folder e.g. Desktop, then run gd_host.exe)\r\n")
        f.write("  gd_gameplay.png                               proof: live GD gameplay rendered at FL 9_1\r\n\r\n")
        f.write("source.zip - the Win32Craft source + tooling to continue (extracts to Win32Craft_src/).\r\n")
        f.write("  No game folders / .appx / .xap are included; use your own game copies to rebuild packages.\r\n")
        f.write("  Build toolchain: clang (thumbv7-pc-windows-msvc) - see HANDOFF_FOR_FRIEND.md > Methods & toolchain.\r\n")

    # sizes
    total = 0
    for dp, _, fns in os.walk(OUT):
        for fn in fns:
            total += os.path.getsize(os.path.join(dp, fn))
    print("bundle: %s  (%.1f MB, source.zip has %d files)" % (OUT, total / 1e6, nsrc))
    for dp, _, fns in os.walk(OUT):
        for fn in sorted(fns):
            p = os.path.join(dp, fn)
            print("  %8.1f MB  %s" % (os.path.getsize(p) / 1e6, os.path.relpath(p, OUT)))
    return 0

if __name__ == "__main__":
    sys.exit(main())
