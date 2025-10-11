#!/usr/bin/env python3
import re, json, pathlib, csv

HERE = pathlib.Path(__file__).parent.resolve()
RAW = HERE / "results" / "raw"
CSV = HERE / "results" / "csv"
CSV.mkdir(parents=True, exist_ok=True)

# REGEX
PATTERNS = {
    # "L1 miss rate: 0.1234" or "L1 MissRate = 12.34%"
    "L1_miss_rate":   re.compile(r"L1\s*miss\s*rate\s*[:=]\s*(\d*\.?\d+)%?", re.I),
    "L2_miss_rate":   re.compile(r"L2\s*miss\s*rate\s*[:=]\s*(\d*\.?\d+)%?", re.I),
    "L1_hits":        re.compile(r"L1\s*hits\s*[:=]\s*(\d+)", re.I),
    "L1_misses":      re.compile(r"L1\s*miss(?:es)?\s*[:=]\s*(\d+)", re.I),
    "L2_hits":        re.compile(r"L2\s*hits\s*[:=]\s*(\d+)", re.I),
    "L2_misses":      re.compile(r"L2\s*miss(?:es)?\s*[:=]\s*(\d+)", re.I),
    "prefetches":     re.compile(r"prefetch(?:es)?\s*[:=]\s*(\d+)", re.I),
    "useful_pref":    re.compile(r"useful\s*prefetch(?:es)?\s*[:=]\s*(\d+)", re.I),
    "pref_acc":       re.compile(r"prefetch\s*accuracy\s*[:=]\s*(\d*\.?\d+)%?", re.I),
    "bytes_read":     re.compile(r"bytes\s*read\s*[:=]\s*(\d+)", re.I),
    "bytes_write":    re.compile(r"bytes\s*writ(?:e|ten)\s*[:=]\s*(\d+)", re.I),
    "amat":           re.compile(r"AMAT\s*[:=]\s*(\d*\.?\d+)", re.I),
    "ipc":            re.compile(r"IPC\s*[:=]\s*(\d*\.?\d+)", re.I),
    "cycles":         re.compile(r"cycles\s*[:=]\s*(\d+)", re.I),
    "instructions":   re.compile(r"instructions\s*[:=]\s*(\d+)", re.I),
}

def parse_one(txt: str):
    out = {}
    for k, rx in PATTERNS.items():
        m = rx.search(txt)
        if m:
            val = m.group(1)
            # Convert 
            if k.endswith("_rate") or k in ("pref_acc","amat","ipc"):
                out[k] = float(val)
                # If miss rate came as percent norm to frac.
                if "rate" in k or k == "pref_acc":
                    if out[k] > 1.0:
                        out[k] /= 100.0
            else:
                out[k] = int(val)
    # Derived
    if "L1_hits" in out and "L1_misses" in out:
        denom = out["L1_hits"] + out["L1_misses"]
        if denom:
            out.setdefault("L1_miss_rate", out["L1_misses"]/denom)
    if "L2_hits" in out and "L2_misses" in out:
        denom = out["L2_hits"] + out["L2_misses"]
        if denom:
            out.setdefault("L2_miss_rate", out["L2_misses"]/denom)
    if "useful_pref" in out and "prefetches" in out and out["prefetches"]>0:
        out.setdefault("pref_acc", out["useful_pref"]/out["prefetches"])
    if "instructions" in out and "L1_misses" in out and out["instructions"]>0:
        out["L1_MPKI"] = 1000.0 * out["L1_misses"] / out["instructions"]
    return out

def main():
    rows = []
    for p in RAW.glob("*.txt"):
        params_file = p.with_suffix(".json")
        if not params_file.exists(): 
            continue
        params = json.loads(params_file.read_text())
        metrics = parse_one(p.read_text())

        # Extract trace name from filename
        trace = p.name.split("__",1)[0].replace("_",".")
        row = {"trace": trace, **params, **metrics}
        rows.append(row)

    # Write master CSV
    if rows:
        keys = sorted({k for r in rows for k in r.keys()})
        with open(CSV / "all_results.csv","w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=keys)
            w.writeheader()
            for r in rows:
                w.writerow(r)
        print("Wrote", CSV / "all_results.csv")
    else:
        print("[WARN] No rows parsed. Did runs complete?")

if __name__ == "__main__":
    main()
