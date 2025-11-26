#!/usr/bin/env python3
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import pathlib, yaml

HERE = pathlib.Path(__file__).parent.resolve()
CSV = HERE / "results" / "csv"
PLOTS = HERE / "plots"
PLOTS.mkdir(parents=True, exist_ok=True)

def load_cfg():
    with open(HERE / "config_graphs.yaml","r") as f:
        return yaml.safe_load(f)

def load_results():
    return pd.read_csv(CSV / "all_results.csv")

def subset(df, hold):
    out = df.copy()
    for k, v in hold.items():
        if k in out.columns:
            out = out[out[k] == v]
        elif k == "trace" and "TRACE" in out.columns:
            out = out[out["TRACE"] == v]
    return out

def maybe_add_axes(df):
    if "L1_SIZE" in df.columns and "log2_L1_SIZE" not in df.columns:
        df["log2_L1_SIZE"] = np.log2(df["L1_SIZE"])
    if "BLOCKSIZE" in df.columns and "log2_BLOCKSIZE" not in df.columns:
        df["log2_BLOCKSIZE"] = np.log2(df["BLOCKSIZE"])

def normalize_trace(df):
    if "trace" in df.columns and "TRACE" not in df.columns:
        df["TRACE"] = df["trace"]
    if "TRACE" in df.columns:
        df["TRACE"] = df["TRACE"].str.replace("_", ".", regex=False)

def mark_fully_associative(df):
    if {"L1_SIZE","L1_ASSOC","BLOCKSIZE"}.issubset(df.columns):
        df["is_FA"] = np.isclose(df["L1_ASSOC"], (df["L1_SIZE"] / df["BLOCKSIZE"]))
    else:
        df["is_FA"] = False
    # Single place to define legend/series labels:
    df["assoc_label"] = np.where(df["is_FA"], "FA", df["L1_ASSOC"].astype(int).astype(str))

def synthesize_amat_if_missing(df, t_L1=1.0, t_L2=8.0, MP=100.0):
    for col in ("L1_miss_rate", "L2_miss_rate"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
            df.loc[df[col] > 1.0, col] = df.loc[df[col] > 1.0, col] / 100.0
    if "amat" not in df.columns or df["amat"].isna().all():
        if "L2_miss_rate" in df.columns and df["L2_miss_rate"].notna().any() and (df.get("L2_SIZE",0) != 0).any():
            df["amat"] = t_L1 + df["L1_miss_rate"] * (t_L2 + df["L2_miss_rate"] * MP)
        else:
            df["amat"] = t_L1 + df["L1_miss_rate"] * MP

def plot_multiseries(df, x, y, series_col, title, outpath, exclude_fa=False):
    plt.figure()
    df = df.sort_values(x)
    if exclude_fa and "is_FA" in df.columns:
        df = df[~df["is_FA"]]
    for s_val, group in df.groupby(series_col):
        group = group.sort_values(x)
        plt.plot(group[x], group[y], marker="o", label=str(s_val))
    plt.xlabel(x)
    plt.ylabel(y)
    plt.title(title)
    plt.legend(title=series_col)
    plt.grid(True, linestyle=":")
    plt.tight_layout()
    plt.savefig(outpath)
    plt.close()

def main():
    cfg = load_cfg()
    df = load_results()
    normalize_trace(df)
    maybe_add_axes(df)
    mark_fully_associative(df)
    synthesize_amat_if_missing(df)

    for fig in cfg["figures"]:
        name   = fig["name"]
        x      = fig["x"]
        y      = fig["y"]
        # Use assoc_label instead of raw L1_ASSOC when the config asks for it:
        series = fig["series"]
        hold   = fig["hold"]
        sub = subset(df, hold)

        if sub.empty:
            print(f"[WARN] No data for {name}")
            continue

        # Decide whether to exclude FA (Graph #2 case)
        exclude_fa = "noFA" in name

        outpath = PLOTS / f"{name}.png"
        plot_multiseries(sub, x, y, series, fig.get("title", name), outpath, exclude_fa=exclude_fa)
        print("Wrote", outpath)

if __name__ == "__main__":
    main()
