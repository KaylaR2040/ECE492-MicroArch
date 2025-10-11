#!/usr/bin/env python3
import pandas as pd
import numpy as np
from pathlib import Path

CSV = Path("results/csv/all_results.csv")
T_L1 = 1.0
T_L2 = 8.0
MEM_PENALTY = 100.0

# Load and preprocess CSV data
def load_csv():
    if not CSV.exists():
        raise SystemExit("ERROR: results/csv/all_results.csv not found. Run parse_results.py first.")
    df = pd.read_csv(CSV)
    if "trace" in df.columns and "TRACE" not in df.columns:
        df["TRACE"] = df["trace"]
    df["TRACE"] = df["TRACE"].str.replace("_",".", regex=False)

    for col in ("L1_miss_rate","L2_miss_rate"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
            df.loc[df[col] > 1.0, col] = df.loc[df[col] > 1.0, col] / 100.0

    if "L1_SIZE" in df.columns: df["log2_L1_SIZE"] = np.log2(df["L1_SIZE"])
    if "BLOCKSIZE" in df.columns: df["log2_BLOCKSIZE"] = np.log2(df["BLOCKSIZE"])

    if "amat" not in df.columns or df["amat"].isna().all():
        if "L2_miss_rate" in df.columns and df["L2_miss_rate"].notna().any() and (df.get("L2_SIZE",0) != 0).any():
            df["amat"] = T_L1 + df["L1_miss_rate"] * (T_L2 + df["L2_miss_rate"] * MEM_PENALTY)
        else:
            df["amat"] = T_L1 + df["L1_miss_rate"] * MEM_PENALTY

    df["is_FA"] = np.isclose(df.get("L1_ASSOC",0), (df.get("L1_SIZE",1) / df.get("BLOCKSIZE",1)))
    return df

# Graph #1: Compulsory Miss Rate Estimate
def graph1_compulsory(df):
    g1 = df[(df["TRACE"]=="gcc.trace.txt") & (df["BLOCKSIZE"]==32) & (df["L2_SIZE"]==0)]
    if g1.empty: return None
    mx = g1["L1_SIZE"].max()
    fa = g1[(g1["L1_SIZE"]==mx) & (g1["is_FA"])]
    mr = fa["L1_miss_rate"].min() if not fa.empty else g1[g1["L1_SIZE"]==mx]["L1_miss_rate"].min()
    return float(mr)

# Graph #2: Best L1 cache, BLK=32, no L2
def graph2_best(df):
    g2 = df[(df["TRACE"]=="gcc.trace.txt") & (df["BLOCKSIZE"]==32) & (df["L2_SIZE"]==0)]
    g2 = g2[~g2["is_FA"]]
    if g2.empty: return None
    best = g2.loc[g2["amat"].idxmin()]
    return int(best["L1_SIZE"]//1024), int(best["L1_ASSOC"]), float(best["amat"])

# Graph #3: Best L1 cache, BLK=32, L2 = 16KB 8-way
def graph3_best(df):
    g3 = df[(df["TRACE"]=="gcc.trace.txt") & (df["BLOCKSIZE"]==32) & (df["L2_SIZE"]==16384) & (df["L2_ASSOC"]==8)]
    g3 = g3[g3["L1_SIZE"].isin([1024,2048,4096,8192]) & g3["L1_ASSOC"].isin([1,2,4,8])]
    if g3.empty: return None
    best = g3.loc[g3["amat"].idxmin()]
    return int(best["L1_SIZE"]//1024), int(best["L1_ASSOC"]), float(best["amat"])

# Graph #4: Best block size, L1=1KB 4-way no L2; and L1 = 32KB 4-way no L2
def graph4_blocks(df):
    g4 = df[(df["TRACE"]=="gcc.trace.txt") & (df["L2_SIZE"]==0) & (df["L1_ASSOC"]==4) & (df["BLOCKSIZE"].isin([16,32,64,128]))]
    if g4.empty: return None
    b1 = int(g4[g4["L1_SIZE"]==1024].loc[lambda g: g["L1_miss_rate"].idxmin()]["BLOCKSIZE"])
    b2 = int(g4[g4["L1_SIZE"]==32768].loc[lambda g: g["L1_miss_rate"].idxmin()]["BLOCKSIZE"])
    return b1, b2

# Graph #5: Best (L1 size, L2 size), BLK = 32, L1 assoc = 4, L2 assoc=8
def graph5_best(df):
    g5 = df[(df["TRACE"]=="gcc.trace.txt") & (df["BLOCKSIZE"]==32) & (df["L1_ASSOC"]==4) & (df["L2_ASSOC"]==8)]
    g5 = g5[g5["L1_SIZE"].isin([1024,2048,4096,8192]) & g5["L2_SIZE"].isin([16384,32768,65536])]
    if g5.empty: return None
    best = g5.loc[g5["amat"].idxmin()]
    return int(best["L1_SIZE"]//1024), int(best["L2_SIZE"]//1024), float(best["amat"])

def main():
    df = load_csv()

    print("=== Graph #1 answers ===")
    print("Q1 text:", "For a fixed associativity, increasing L1 size reduces miss rate with diminishing returns; beyond tens of KB it approaches the compulsory floor.")
    print("Q2 text:", "For a fixed L1 size, increasing associativity reduces conflict misses; DM→2-way is the biggest drop, 2→4 smaller, 4→8 marginal.")
    c = graph1_compulsory(df)
    print("Q3 compulsory miss-rate estimate:", c)

    print("\n=== Graph #2 answers ===")
    b2 = graph2_best(df)
    print("(best L1 size KB, assoc, AAT):", b2)

    print("\n=== Graph #3 answers ===")
    b3 = graph3_best(df)
    if b3 is None:
        print("Missing Graph #3 data. Run the Graph #3 sweep in config.yaml (L2=16KB, 8-way; BLK=32; L1 sizes 1..8KB; assoc 1,2,4,8).")
    else:
        print("(best L1 size KB, assoc, AAT):", b3)

    print("\n=== Graph #4 answers ===")
    g4 = graph4_blocks(df)
    print("(best block for 1KB, best block for 32KB):", g4)

    print("\n=== Graph #5 answers ===")
    b5 = graph5_best(df)
    print("(best L1 size KB, best L2 size KB, AAT):", b5)

if __name__ == "__main__":
    main()
