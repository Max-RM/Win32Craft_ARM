#!/usr/bin/env python3
"""Coin-pack patch for Hill Climb Racing 1.27.0.32.

The store check in the coin-pack click handler is skipped and the handler is
sent into a cave. The cave parses the digits after the first '_' of the
product id (com.fingersoft.hillclimb.adfree_<amount>coins) and adds that
amount, with ldrex/strex, to a dword at preferred VA 0x51AFFC (RVA 0x11AFFC,
the last dword of the same .text page). It returns without calling the game.

The host spends that dword on the next UI pass, after the button handler has
returned: sub_41E240 on the live coin widget, or a saturating add to the coins
global at RVA 0x1A6630 when the widget is not up. Calling either from inside
the button crashes. The host also makes this page writable; the dword is
PC-relative, so the queue does not need a relocation.

File offsets are VA - 0x400C00. The cave sits in .text padding at file
0x11A360, past the host trampoline at file 0x11A350.
"""
from __future__ import annotations

import json
from pathlib import Path

IMAGE_BASE = 0x400000
FILE_DELTA = 0xC00  # file offset = VA - 0x400C00

BRANCH_OFF = 0x2B050
BRANCH_OLD = bytes.fromhex("C3B1")  # cbz r3, fail
BRANCH_NEW = bytes.fromhex("18E0")  # b success

ENTRY_OFF = 0x8F38
ENTRY_OLD = bytes.fromhex("2DE99048")  # push.w {r4, r7, fp, lr}
ENTRY_NEW = bytes.fromhex("11F112BA")  # b.w cave

CAVE_OFF = 0x11A360
# Queue the pack for the host. No game call: the button handler only returns.
CAVE_NEW = bytes.fromhex(
    "70b50446002614f8013b5f2bfbd1002514f8013b303b092b04d8"
    "0a2006fb00360135f5e765b11da050e8003f9b1903d74ff6ff73"
    "c7f6ff7340e800310029f3d170bd"
)
# Previous cave: call sub_41E240 from the click. The game closes.
CAVE_WIDGET = bytes.fromhex(
    "70b50446002614f8013b5f2bfbd1002514f8013b303b092b04d8"
    "0a2006fb00360135f5e705b3eef609fe46f20060c0f25a000068"
    "60b1d0f8540148b1d0f8603133b1d3f864311bb1314603f749f9"
    "0be046f23064c0f25a0423689b1903d74ff6ff73c7f6ff732360"
    "70bd"
)
# Previous cave: queue the pack and return. The poller did not grant it.
CAVE_POLL = bytes.fromhex(
    "70b50446002614f8013b5f2bfbd1002514f8013b303b092b04d8"
    "0a2006fb00366d1cf5e72db1eef64dfd31460022c4f789ff70bd"
)
# Immediate grant from the click: calls the purchase-complete UI and crashes.
CAVE_CRASH = bytes.fromhex(
    "70b50446002614f8013b5f2bfbd1002514f8013b303b092b04d8"
    "0a2006fb00366d1cf5e72db1eef64dfd31460022c4f789fff0f64dfe70bd"
)
# Immediate add + save from the click. The save re-enters the UI and crashes.
CAVE_DIRECT = bytes.fromhex(
    "70b50446002614f8013b5f2bfbd1002514f8013b303b092b04d8"
    "0a2006fb00366d1cf5e775b146f23064c0f25a0423689b1903d7"
    "4ff6ff73c7f6ff732360012087f748fb70bd"
)

SWITCH_TITLE = "GET COINS: free coin packs (no Store)"
EXE_NAME = "Win32Craft.HillClimbRacing.exe"


def config_dict() -> dict:
    patches = []

    def add(off: int, old: bytes, new: bytes) -> None:
        if len(old) != len(new):
            raise ValueError("old/new length mismatch at %#x" % off)
        for index, (was, now) in enumerate(zip(old, new)):
            patches.append("%#x >> %02X >> %02X" % (off + index, was, now))

    add(BRANCH_OFF, BRANCH_OLD, BRANCH_NEW)
    add(ENTRY_OFF, ENTRY_OLD, ENTRY_NEW)
    add(CAVE_OFF, bytes(len(CAVE_NEW)), CAVE_NEW)
    return {
        "enableAll": False,
        "enableRAM": False,
        "RAMWarningDisabled": True,
        "path": EXE_NAME,
        "switches": [{"title": SWITCH_TITLE, "patches": patches}],
    }


def config_text() -> str:
    return json.dumps(config_dict(), indent=4) + "\n"


def _cave_span() -> int:
    return max(len(CAVE_NEW), len(CAVE_WIDGET), len(CAVE_POLL), len(CAVE_CRASH), len(CAVE_DIRECT))


def _cave_known(have: bytes) -> bool:
    if have[:len(CAVE_NEW)] == CAVE_NEW and have[len(CAVE_NEW):] == bytes(len(have) - len(CAVE_NEW)):
        return True
    if have == bytes(len(have)):
        return True
    for previous in (CAVE_WIDGET, CAVE_POLL, CAVE_CRASH, CAVE_DIRECT):
        if have[:len(previous)] == previous and have[len(previous):] == bytes(len(have) - len(previous)):
            return True
    return False


def apply_coins(exe: bytearray) -> str:
    """Write the ON state. Returns 'applied', 'already', or raises ValueError.

    The branch and the handler entry are patched from their stock bytes. The
    cave is written when it is still padding, or when it still holds one of
    the two earlier stubs.
    """
    changed = False

    def expect(off: int, old: bytes, new: bytes, label: str) -> None:
        nonlocal changed
        have = bytes(exe[off:off + len(new)])
        if have == new:
            return
        if have != old:
            raise ValueError(
                "%s at %#x is %s, expected %s or %s"
                % (label, off, have.hex(), old.hex(), new.hex())
            )
        exe[off:off + len(new)] = new
        changed = True

    expect(BRANCH_OFF, BRANCH_OLD, BRANCH_NEW, "store branch")
    expect(ENTRY_OFF, ENTRY_OLD, ENTRY_NEW, "handler entry")
    span = _cave_span()
    have = bytes(exe[CAVE_OFF:CAVE_OFF + span])
    want = CAVE_NEW + bytes(span - len(CAVE_NEW))
    if have != want:
        if not _cave_known(have):
            raise ValueError("cave at %#x is %s" % (CAVE_OFF, have[:16].hex()))
        exe[CAVE_OFF:CAVE_OFF + span] = want
        changed = True
    return "applied" if changed else "already"


def write_config(path: Path) -> None:
    path.write_text(config_text(), encoding="utf-8", newline="\n")
