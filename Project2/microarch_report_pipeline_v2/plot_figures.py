#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import pathlib, yaml

HERE = pathlib.Path(__file__).parent.resolve()
CSV = HERE / "results" / "csv"
PLOTS = HERE / "plots"
PLOTS.mkdir(parents=True, exist_ok=True)

def load_cfg():
    with open(HERE / "config.yaml","r") as f:
        return yaml.safe_load(f)

def load_results():
    df = pd.read_csv(CSV / "all_results.csv")
    return df

def plot_series(df, xcol, ycol, title, outpath):
    plt.figure()
    df = df.sort_values(xcol)
    plt.plot(df[xcol], df[ycol], marker="o")
    plt.xlabel(xcol)
    plt.ylabel(ycol)
    plt.title(title)
    plt.grid(True, which="both", linestyle=":")
    plt.tight_layout()
    plt.savefig(outpath)
    plt.close()

def subset_for(fig, df, hold, vary):
    # filter by 'hold' values, then keep columns [vary, y] for plotting
    sub = df.copy()
    for k,v in hold.items():
        sub = sub[sub[k] == v]
    return sub

def main():
    cfg = load_cfg()
    df = load_results()

    # Common choices for y-axis; adjust per your report
    candidates = [
        ("L1_miss_rate", "L1 Miss Rate"),
        ("L2_miss_rate", "L2 Miss Rate"),
        ("pref_acc", "Prefetch Accuracy"),
        ("amat", "AMAT"),
        ("ipc", "IPC"),
        ("L1_MPKI", "L1 MPKI"),
    ]

    for fig in cfg.get("figures", []):
        name = fig["name"]
        vary = fig["vary"]
        hold = fig["hold"]
        sub = subset_for(fig, df, hold, vary)
        if sub.empty:
            print(f"[WARN] No data for {name}. Check your grid/hold values.")
            continue
        for ycol, yname in candidates:
            if ycol in sub.columns:
                plot_series(sub, vary, ycol, f"{yname} vs {vary} ({name})", PLOTS / f"{name}__{ycol}.png")
                print("Wrote", PLOTS / f"{name}__{ycol}.png")

if __name__ == "__main__":
    main()
