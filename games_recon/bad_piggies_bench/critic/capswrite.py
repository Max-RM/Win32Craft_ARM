"""capsuse.py <caps-offset-hex> [maxwin]: list every caller of the caps getter sub_733428 whose code reads
[caps + off] shortly after the call, with a disassembly window (to find who consumes a caps field)."""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import arm32re
off = int(sys.argv[1], 16)
win = int(sys.argv[2]) if len(sys.argv) > 2 else 10
c = arm32re.Ctx(); pe = c.pe; ann = c.annotator()
funcs = sorted({c.func_of(s)[0] for s, k in c.idx.callers.get(0x733428, []) if c.func_of(s)})
pat = re.compile(r"\[r\d+, #0x%x\]" % off)
for f in funcs:
    b, l = c.func_of(f)
    lines = arm32re.thumb_dis(pe, b, count=100000, end=b + l, ann=ann)
    for i, ln in enumerate(lines):
        if "sub_733428" in ln:
            for j in range(i + 1, min(i + 12, len(lines))):
                if pat.search(lines[j]) and "str" in lines[j]:
                    print("== %s" % c.fname(b))
                    for k in range(i, min(j + win, len(lines))): print("   " + lines[k])
                    break
