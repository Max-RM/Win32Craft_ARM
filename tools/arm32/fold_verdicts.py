#!/usr/bin/env python3
"""Fold the independent verifier verdicts into a facts directory so gen_constants.py only emits trustworthy numbers.

    python tools/arm32/fold_verdicts.py <facts-dir> <out-dir>

For every <area>.json + <area>.verdicts.json in <facts-dir>:
  CONFIRMED                       fact kept unchanged
  CORRECTED + corrected_arm_rva   arm_rva replaced by the verifier's value (original kept in `finder_arm_rva`)
  CORRECTED without a value       fact kept, `verifier_note` added (the verifier corrected the description, not the number)
  REFUTED                         arm_rva / expected_bytes removed (the fact is NOT emitted into the header), `refuted_note` added
  UNVERIFIABLE                    arm_rva / expected_bytes removed, `unverified_note` added
  no verdict                      fact kept, `verdict` = "none"
Non-fact files (verdicts, audit.json) are not copied.  Prints a summary per area."""
import json
import os
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    total = {"CONFIRMED": 0, "CORRECTED": 0, "REFUTED": 0, "UNVERIFIABLE": 0, "none": 0, "rva_replaced": 0, "rva_dropped": 0}
    for name in sorted(os.listdir(src)):
        if not name.endswith(".json") or name.endswith(".verdicts.json") or name in ("audit.json", "imports_table.json"):
            continue
        area = name[:-5]
        facts = json.load(open(os.path.join(src, name), encoding="utf-8"))
        vpath = os.path.join(src, area + ".verdicts.json")
        verdicts = {}
        if os.path.exists(vpath):
            for v in json.load(open(vpath, encoding="utf-8")).get("verdicts", []):
                verdicts[v.get("id")] = v
        for fact in facts.get("facts", []):
            v = verdicts.get(fact.get("id"))
            kind = v.get("verdict") if v else "none"
            fact["verdict"] = kind
            total[kind] = total.get(kind, 0) + 1
            if v is None or kind == "CONFIRMED":
                continue
            note = v.get("note", "")
            if kind == "CORRECTED":
                new_rva = v.get("corrected_arm_rva")
                if new_rva:
                    fact["finder_arm_rva"] = fact.get("arm_rva")
                    fact["arm_rva"] = new_rva
                    total["rva_replaced"] += 1
                fact["verifier_note"] = note
            else:
                key = "refuted_note" if kind == "REFUTED" else "unverified_note"
                fact[key] = note
                if fact.get("arm_rva") is not None or fact.get("expected_bytes") is not None:
                    total["rva_dropped"] += 1
                fact["finder_arm_rva"] = fact.get("arm_rva")
                fact["arm_rva"] = None
                fact["expected_bytes"] = None
        json.dump(facts, open(os.path.join(dst, name), "w", encoding="utf-8"), indent=1)
    print("folded:", total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
