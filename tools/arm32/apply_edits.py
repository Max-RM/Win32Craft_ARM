#!/usr/bin/env python3
"""Deterministic applier for exact-match edit pairs.

Layout of an edit unit directory (arm32/edits/<unit>/):
    NNN.old.txt   exact text to find (must occur EXACTLY once in the target at the time it is applied)
    NNN.new.txt   replacement text
    NNN.file.txt  optional: target file name relative to the repo root (default: win32_host.c)
    NNN.note.txt  optional: rationale (ignored by the applier)
Edits are applied in lexicographic order of NNN.  Line endings are normalised to the target's style
(the repo sources are CRLF), so write the pair with plain newlines.  Whitespace otherwise must match exactly.

Units are applied in the order given on the command line; successful edits stay applied to the in-memory copy so
later edits still see earlier ones.  Exit code 1 on any failure (failures are listed).

Modes:
    --workdir DIR   copy the pristine inputs (arm32/orig/<name> when present, else the repo file) into DIR,
                    apply every edit there, write the edited files into DIR (never touches the repo)
    --inplace       apply to the real repo files (integration step)
    --check         validate only
"""
import sys, argparse
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CRLF = "\r\n"
LF = "\n"


def load_pairs(unit_dir):
    d = Path(unit_dir)
    if not d.is_dir():
        d = REPO / "arm32" / "edits" / str(unit_dir)
    ids = sorted({p.name.split('.')[0] for p in d.glob('*.old.txt')})
    for i in ids:
        old = (d / (i + '.old.txt')).read_bytes().decode('utf-8')
        newp = d / (i + '.new.txt')
        if not newp.exists():
            yield d.name, i, None, None, None, 'missing %s.new.txt' % i
            continue
        new = newp.read_bytes().decode('utf-8')
        fp = d / (i + '.file.txt')
        target = fp.read_text(encoding='utf-8').strip() if fp.exists() else 'win32_host.c'
        yield d.name, i, target, old, new, None


def pristine_path(target):
    orig = REPO / 'arm32' / 'orig' / target
    return orig if orig.exists() else REPO / target


def apply_units(units, check_only=False, inplace=False, workdir=None):
    state = {}
    failures = []
    applied = 0
    for u in units:
        for uname, i, target, old, new, err in load_pairs(u):
            tag = '%s/%s' % (uname, i)
            if err:
                failures.append((tag, err))
                continue
            if target not in state:
                src = (REPO / target) if inplace else pristine_path(target)
                if not src.exists():
                    failures.append((tag, 'target missing: %s' % src))
                    continue
                state[target] = src.read_bytes().decode('utf-8')
            text = state[target]
            eol = CRLF if CRLF in text else LF
            old = old.replace(CRLF, LF).replace(LF, eol)
            new = new.replace(CRLF, LF).replace(LF, eol)
            n = text.count(old)
            if n != 1:
                failures.append((tag, 'old text occurs %d times in %s (need exactly 1): %r' % (n, target, old[:90])))
                continue
            state[target] = text.replace(old, new, 1)
            applied += 1
    return state, failures, applied


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('units', nargs='+')
    ap.add_argument('--workdir')
    ap.add_argument('--inplace', action='store_true')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    state, failures, applied = apply_units(a.units, a.check, a.inplace, a.workdir)
    for tag, err in failures:
        print('FAIL', tag, err)
    print('applied %d edits, %d failures' % (applied, len(failures)))
    if not a.check:
        if a.workdir:
            wd = Path(a.workdir)
            wd.mkdir(parents=True, exist_ok=True)
            # unedited inputs the compile needs, then the edited files
            for name in ('win32_host.c', 'api_compat.c'):
                src = pristine_path(name)
                if name not in state and src.exists():
                    state[name] = src.read_bytes().decode('utf-8')
            for t, txt in state.items():
                (wd / t).parent.mkdir(parents=True, exist_ok=True)
                (wd / t).write_bytes(txt.encode('utf-8'))
            print('wrote', ', '.join(sorted(state)), 'to', wd)
        elif a.inplace:
            for t, txt in state.items():
                (REPO / t).write_bytes(txt.encode('utf-8'))
                print('wrote', t)
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
