#!/usr/bin/env python3
"""Old-versus-new proof for host refactors: run the x86 integration harness (test_0104_all.exe, built by
`try_0104.py --integration`) in verbose mode, normalise run-dependent values (pointers, handles, thread ids,
tick counts, frame counters) and compare with a saved baseline.

    python tools/arm32/diff_0104_log.py --save arm32/work/baseline_pre_redcon/integration_set.txt --runs 12   # freeze
    python tools/arm32/diff_0104_log.py --against arm32/work/baseline_pre_redcon/integration_set.txt            # compare

Exit code 0 = identical after normalisation (or saved)."""
import argparse, re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
EXE = REPO / "arm32" / "work" / "_test_0104_test_0104_all" / "test_0104_all.exe"

SUBS = [
    (re.compile(r"(frames|passes|events|ticks|heartbeat|count|elapsed)=\d+"), r"\1=N"),
    (re.compile(r"\(serial \d+\)"), "(serial N)"),
    (re.compile(r"passes \d+"), "passes N"),
    (re.compile(r"(workers running|cancelled) \d+"), r"\1 N"),   # shutdown races with worker exit
    (re.compile(r"\d+ checks, 0 failures"), "N checks, 0 failures"),
    (re.compile(r"\b0x[0-9A-Fa-f]{5,16}\b"), "0xH"),
    (re.compile(r"\b[0-9A-Fa-f]{8}\b"), "H8"),
    (re.compile(r"\b[0-9A-Fa-f]{16}\b"), "H16"),
    (re.compile(r"(ui thread|thread|tid|T)[ =]?[0-9a-fA-F]{2,5}\b"), r"\1 TID"),
    (re.compile(r"\b\d+(\.\d+)? ?ms\b"), "N ms"),
    (re.compile(r"(frames|passes|events|ticks|heartbeat|count|elapsed)=\d+"), r"\1=N"),
    (re.compile(r"(window|hwnd|HWND) [0-9A-Fa-f]{6,8}\b"), r"\1 HWND"),
]


def normalise(text):
    """only the host log lines ([log]) and the result line; worker threads interleave, so the result is SORTED
    (a multiset comparison: every line must still appear exactly as often, only the order between threads is free)"""
    out = []
    for line in text.splitlines():
        if "[log]" not in line and "== integration test:" not in line:
            continue
        for rx, rep in SUBS:
            line = rx.sub(rep, line)
        out.append(line.rstrip())
    return sorted(out)


def run_once():
    r = subprocess.run([str(EXE), "v"], capture_output=True, text=True, timeout=120)
    return r.returncode, normalise((r.stdout or "") + (r.stderr or ""))


def collect(runs):
    """the harness has a few timing-dependent lines (which WM_CHAR arrives before text mode, a watchdog that fires
    on a slow pass ...): the fingerprint is the SET of distinct normalised lines seen over several runs"""
    seen, rcs = set(), []
    for _ in range(runs):
        rc, lines = run_once()
        rcs.append(rc)
        seen.update(lines)
    return seen, rcs


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--save")
    g.add_argument("--against")
    ap.add_argument("--runs", type=int, default=8)
    a = ap.parse_args()
    seen, rcs = collect(a.runs)
    if any(rcs):
        print("harness FAILED in some runs: rc=%s" % rcs)
    if a.save:
        Path(a.save).write_text("\n".join(sorted(seen)) + "\n", encoding="utf-8")
        print("saved %d distinct normalised lines from %d runs (rc %s) to %s" % (len(seen), a.runs, rcs, a.save))
        return 1 if any(rcs) else 0
    base = set(Path(a.against).read_text(encoding="utf-8").splitlines())
    only_new, only_base = sorted(seen - base), sorted(base - seen)
    if not only_new and not only_base and not any(rcs):
        print("IDENTICAL: %d distinct normalised lines over %d runs" % (len(seen), a.runs))
        return 0
    print("DIFFERENT: %d lines only now, %d lines only in the baseline (rc %s)" % (len(only_new), len(only_base), rcs))
    for l in only_new[:100]:
        print("+ " + l)
    for l in only_base[:100]:
        print("- " + l)
    print("(a line that is only on one side may still be a timing variant: re-run with a larger --runs before concluding)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
