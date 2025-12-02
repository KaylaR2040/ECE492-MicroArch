#!/usr/bin/env python3
"""
Generate data products for ECE 463 Project 2 report.

This script runs the provided branch predictor simulator across the required
configuration sweeps, captures the outputs, builds the tables/figures needed
for the report, and emits a Markdown file that mirrors the official template.

Outputs are written under ./report_assets and ./report.md.
"""
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List

import matplotlib.pyplot as plt
import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parent
C_FILES_DIR = PROJECT_ROOT / "c_files"
SIM_PATH = C_FILES_DIR / "sim"
TRACE_DIR = PROJECT_ROOT / "test" / "proj2-traces"
OUTPUT_DIR = PROJECT_ROOT / "report_assets"

# Sweep parameters required by the project specification.
BIMODAL_M_VALUES = list(range(7, 21))
BENCHMARKS: Dict[str, str] = {
    "gcc": "gcc_trace.txt",
    "jpeg": "jpeg_trace.txt",
    "perl": "perl_trace.txt",
}


@dataclass
class SimRun:
    predictions: int
    mispredictions: int
    misprediction_rate: float
    raw_stdout: str


def ensure_environment() -> None:
    """Validate that simulator binary and trace files exist."""
    if not SIM_PATH.exists():
        raise FileNotFoundError(
            f"Simulator binary not found at {SIM_PATH}. "
            "Run `make` inside c_files/ before executing this script."
        )
    if not TRACE_DIR.exists():
        raise FileNotFoundError(
            f"Trace directory not found at {TRACE_DIR}. "
            "Ensure the provided traces are available."
        )
    missing_traces = [
        name for name in BENCHMARKS.values()
        if not (TRACE_DIR / name).exists()
    ]
    if missing_traces:
        raise FileNotFoundError(
            "One or more required trace files are missing from "
            f"{TRACE_DIR}. Missing: {', '.join(missing_traces)}."
        )


def run_simulator(args: List[str]) -> SimRun:
    """Invoke the simulator with the given argument list and parse its output."""
    command = [str(SIM_PATH)] + args
    result = subprocess.run(
        command,
        cwd=C_FILES_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=True,
    )

    predictions_match = re.search(
        r"number of predictions:\s+(\d+)", result.stdout)
    mispred_match = re.search(
        r"number of mispredictions:\s+(\d+)", result.stdout)
    rate_match = re.search(
        r"misprediction rate:\s+([0-9]+\.[0-9]+)%", result.stdout)

    if not (predictions_match and mispred_match and rate_match):
        raise ValueError(
            "Unexpected simulator output format. Output was:\n"
            f"{result.stdout}"
        )

    return SimRun(
        predictions=int(predictions_match.group(1)),
        mispredictions=int(mispred_match.group(1)),
        misprediction_rate=float(rate_match.group(1)),
        raw_stdout=result.stdout,
    )


def gather_bimodal_data() -> pd.DataFrame:
    """Sweep bimodal configurations for all benchmarks."""
    records: List[Dict[str, object]] = []
    for benchmark, trace_name in BENCHMARKS.items():
        trace_path = str((TRACE_DIR / trace_name).resolve())
        for m_bits in BIMODAL_M_VALUES:
            sim_args = ["bimodal", str(m_bits), trace_path]
            sim_result = run_simulator(sim_args)
            records.append(
                {
                    "benchmark": benchmark,
                    "m": m_bits,
                    "predictions": sim_result.predictions,
                    "mispredictions": sim_result.mispredictions,
                    "misprediction_rate": sim_result.misprediction_rate,
                }
            )
    df = pd.DataFrame.from_records(records)
    return df.sort_values(["benchmark", "m"]).reset_index(drop=True)


def gather_gshare_data() -> pd.DataFrame:
    """Sweep gshare configurations for gcc trace."""
    records: List[Dict[str, object]] = []
    trace_path = str((TRACE_DIR / BENCHMARKS["gcc"]).resolve())
    for m_bits in BIMODAL_M_VALUES:
        for n_bits in range(0, m_bits + 1):
            sim_args = ["gshare", str(m_bits), str(n_bits), trace_path]
            sim_result = run_simulator(sim_args)
            records.append(
                {
                    "benchmark": "gcc",
                    "m": m_bits,
                    "n": n_bits,
                    "predictions": sim_result.predictions,
                    "mispredictions": sim_result.mispredictions,
                    "misprediction_rate": sim_result.misprediction_rate,
                }
            )
    df = pd.DataFrame.from_records(records)
    return df.sort_values(["m", "n"]).reset_index(drop=True)


def plot_bimodal_curves(bimodal_df: pd.DataFrame) -> None:
    """Create per-benchmark bimodal plots."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for benchmark in BENCHMARKS:
        subset = bimodal_df[bimodal_df["benchmark"] == benchmark].copy()
        subset.sort_values("m", inplace=True)

        plt.figure(figsize=(6, 4))
        plt.plot(
            subset["m"],
            subset["misprediction_rate"],
            marker="o",
            linewidth=2,
            label=f"{benchmark} bimodal",
        )
        plt.title(f"{benchmark}, bimodal")
        plt.xlabel("m (PC index bits)")
        plt.ylabel("Misprediction rate (%)")
        plt.grid(True, linestyle="--", linewidth=0.5, alpha=0.6)
        plt.tight_layout()
        output_path = OUTPUT_DIR / f"bimodal_{benchmark}.png"
        plt.savefig(output_path, dpi=300)
        plt.close()


def plot_gshare_curves(gshare_df: pd.DataFrame) -> None:
    """Create the multi-curve gshare plot for gcc."""
    plt.figure(figsize=(10, 6))
    for m_bits in BIMODAL_M_VALUES:
        subset = gshare_df[gshare_df["m"] == m_bits].copy()
        subset.sort_values("n", inplace=True)
        label = f"m={m_bits}"
        plt.plot(
            subset["n"],
            subset["misprediction_rate"],
            marker="o",
            linewidth=1.6,
            label=label,
        )

    plt.title("gcc, gshare")
    plt.xlabel("n (global history bits)")
    plt.ylabel("Misprediction rate (%)")
    plt.grid(True, linestyle="--", linewidth=0.5, alpha=0.6)
    plt.legend(
        title="Table size (m bits)",
        bbox_to_anchor=(1.02, 1),
        loc="upper left",
        borderaxespad=0,
        fontsize="small",
    )
    plt.tight_layout()
    output_path = OUTPUT_DIR / "gshare_gcc.png"
    plt.savefig(output_path, dpi=300)
    plt.close()


def format_float(value: float) -> str:
    """Format floating point numbers with two decimal places."""
    return f"{value:.2f}"


def build_bimodal_summary(bimodal_df: pd.DataFrame) -> pd.DataFrame:
    """Summarize minimum misprediction rate per benchmark."""
    rows = []
    for benchmark in BENCHMARKS:
        subset = bimodal_df[bimodal_df["benchmark"] == benchmark]
        min_rate = subset["misprediction_rate"].min()
        min_rows = subset[subset["misprediction_rate"] == min_rate]
        min_m = int(min_rows["m"].min())
        rows.append(
            {
                "benchmark": benchmark,
                "min_m": min_m,
                "min_rate": min_rate,
            }
        )
    return pd.DataFrame(rows)


def build_gshare_summary(gshare_df: pd.DataFrame) -> pd.DataFrame:
    """Summarize best global history length per table size."""
    rows = []
    for m_bits in BIMODAL_M_VALUES:
        subset = gshare_df[gshare_df["m"] == m_bits]
        min_rate = subset["misprediction_rate"].min()
        min_rows = subset[subset["misprediction_rate"] == min_rate]
        best_n = int(min_rows["n"].min())
        bimodal_rate = subset[subset["n"] == 0]["misprediction_rate"].iloc[0]
        rows.append(
            {
                "m": m_bits,
                "best_n": best_n,
                "best_rate": min_rate,
                "bimodal_rate": bimodal_rate,
            }
        )
    return pd.DataFrame(rows)


def derive_textual_observations(
    bimodal_summary: pd.DataFrame,
    gshare_summary: pd.DataFrame,
) -> Dict[str, str]:
    """Compute the narrative answers for analysis questions."""
    observations: Dict[str, str] = {}

    # Bimodal trend statement.
    observations["bimodal_trend"] = (
        "generally decreases as the table grows and then levels off once the "
        "table is large enough to eliminate interference"
    )

    # Bimodal question 3 blank: dedicated entry (2-bit counter).
    observations["bimodal_dedicated"] = "entry (2-bit counter)"

    # Bimodal question 4 inference comparing gcc vs jpeg.
    gcc_min_m = int(
        bimodal_summary[bimodal_summary["benchmark"] == "gcc"]["min_m"].iloc[0]
    )
    jpeg_min_m = int(
        bimodal_summary[bimodal_summary["benchmark"] == "jpeg"]["min_m"].iloc[0]
    )
    if gcc_min_m > jpeg_min_m:
        observations["bimodal_infer_branches"] = (
            "gcc has more static branches than jpeg because gcc requires more "
            "table entries than jpeg before its misprediction rate bottoms out."
        )
    else:
        observations["bimodal_infer_branches"] = (
            "gcc has fewer static branches than jpeg because gcc requires fewer "
            "table entries than jpeg before its misprediction rate bottoms out."
        )

    # Gshare analysis statements.
    small_row = gshare_summary.iloc[0]
    large_row = gshare_summary.iloc[-1]
    small_best_n = small_row["best_n"]
    small_bimodal = small_row["bimodal_rate"]
    small_best_rate = small_row["best_rate"]
    if small_best_n == 0 or small_best_rate >= small_bimodal - 1e-6:
        observations["gshare_small_history"] = (
            "At small table sizes, global history can hurt accuracy. "
            "This is because there are too few counters to accommodate the "
            "extra indexing pressure from using history bits."
        )
    else:
        observations["gshare_small_history"] = (
            "At small table sizes, global history can help accuracy. "
            "This is because there are still abundant counters available "
            "even after incorporating history."
        )

    large_best_n = large_row["best_n"]
    large_bimodal = large_row["bimodal_rate"]
    large_best_rate = large_row["best_rate"]
    if large_best_n > 0 and large_best_rate < large_bimodal - 1e-6:
        observations["gshare_large_history"] = (
            "At large table sizes, global history helps accuracy. "
            "This is because there are abundant counters, so specializing "
            "by history yields additional wins."
        )
    else:
        observations["gshare_large_history"] = (
            "At large table sizes, global history does not provide additional "
            "benefit because the bimodal configuration already saturates the "
            "available counters."
        )

    # Best bimodal overall (smallest rate, smallest m tie breaker).
    observations["best_bimodal"] = ""
    observations["best_gshare"] = ""

    return observations


def select_best_configurations(
    bimodal_df: pd.DataFrame, gshare_df: pd.DataFrame
) -> Dict[str, Dict[str, object]]:
    """Identify best-performing configurations."""
    best_results: Dict[str, Dict[str, object]] = {}

    best_bimodal_idx = bimodal_df["misprediction_rate"].idxmin()
    best_bimodal_row = bimodal_df.loc[best_bimodal_idx]
    best_results["bimodal"] = {
        "benchmark": best_bimodal_row["benchmark"],
        "m": int(best_bimodal_row["m"]),
        "rate": best_bimodal_row["misprediction_rate"],
    }

    best_gshare_idx = gshare_df["misprediction_rate"].idxmin()
    best_gshare_row = gshare_df.loc[best_gshare_idx]
    best_results["gshare"] = {
        "m": int(best_gshare_row["m"]),
        "n": int(best_gshare_row["n"]),
        "rate": best_gshare_row["misprediction_rate"],
    }

    return best_results


def render_markdown(
    bimodal_df: pd.DataFrame,
    gshare_df: pd.DataFrame,
    bimodal_summary: pd.DataFrame,
    gshare_summary: pd.DataFrame,
    observations: Dict[str, str],
    best_configs: Dict[str, Dict[str, object]],
) -> None:
    """Emit report.md that mirrors the provided template with filled content."""
    bimodal_table_lines = ["| Benchmark | m at minimum | Minimum misprediction rate (%) |"]
    bimodal_table_lines.append("|-----------|--------------|-------------------------------|")
    for _, row in bimodal_summary.sort_values("benchmark").iterrows():
        bimodal_table_lines.append(
            f"| {row['benchmark']} | m = {int(row['min_m'])} "
            f"| {format_float(row['min_rate'])} |"
        )

    gshare_table_lines = [
        "| m | Best n | Lowest misprediction rate (%) | Bimodal misprediction rate (%) |"
    ]
    gshare_table_lines.append("|---|--------|------------------------------|------------------------------|")
    for _, row in gshare_summary.iterrows():
        gshare_table_lines.append(
            f"| {int(row['m'])} | {int(row['best_n'])} | "
            f"{format_float(row['best_rate'])} | "
            f"{format_float(row['bimodal_rate'])} |"
        )

    best_bimodal = best_configs["bimodal"]
    best_gshare = best_configs["gshare"]

    markdown_parts: List[str] = []
    markdown_parts.extend(
        [
            "# NC State University",
            "Department of Electrical and Computer Engineering  ",
            "ECE 463/563 (Prof. Rotenberg)  ",
            "Project #2: Branch Prediction  ",
            "",
            "**Student:** << YOUR NAME HERE >>",
            "**NCSU Honor Pledge:** \"I have neither given nor received unauthorized aid on this project.\"",
            "**Student's electronic signature:** ____________________________  ",
            "**Course number:** 463  ",
            "",
            "---",
            "",
            "## Grading Breakdown, Experiments, and Report",
            "",
            "### Part 1: Bimodal Predictor",
            "",
            "#### Experiments",
            "Bimodal predictor sweeps were executed for m = 7 through m = 20 on the gcc, jpeg, "
            "and perl traces using the provided simulator (`./sim`). The misprediction rate for each "
            "configuration is plotted below.",
            "",
            "![gcc bimodal](report_assets/bimodal_gcc.png)",
            "![jpeg bimodal](report_assets/bimodal_jpeg.png)",
            "![perl bimodal](report_assets/bimodal_perl.png)",
            "",
            "#### Analysis",
            f"1. As the bimodal predictor's table size increases, the branch misprediction rate {observations['bimodal_trend']}.",
            "",
            "2. Minimum misprediction points:",
            "",
        ]
    )
    markdown_parts.extend(bimodal_table_lines)
    markdown_parts.extend(
        [
            "",
            f"3. At some point, increasing the bimodal predictor's table size is of no value. "
            f"At this point, each static branch is allocated a dedicated {observations['bimodal_dedicated']} "
            "in the table. Given that interference among different static branches is eliminated at this point, "
            "the only way to improve accuracy further is a better prediction algorithm.",
            "",
            f"4. {observations['bimodal_infer_branches']}",
            "",
            "### Part 2: Gshare Predictor",
            "",
            "#### Experiments",
            "Gshare predictor sweeps were executed for m = 7 through m = 20, with global history lengths "
            "n = 0 through n = m using the gcc trace. The resulting misprediction rates are shown below.",
            "",
            "![gcc gshare](report_assets/gshare_gcc.png)",
            "",
            "#### Analysis",
            f"1. {observations['gshare_small_history']}",
            "",
            f"2. {observations['gshare_large_history']}",
            "",
            "3. Summary table of optimal global history length per table size:",
            "",
        ]
    )
    markdown_parts.extend(gshare_table_lines)
    markdown_parts.extend(
        [
            "",
            f"4. The smallest bimodal predictor that achieves the best accuracy overall occurs on the "
            f"{best_bimodal['benchmark']} trace with m = {best_bimodal['m']} and misprediction rate "
            f"{format_float(best_bimodal['rate'])}%.",
            "",
            f"5. The smallest gshare predictor that achieves the best accuracy overall is m = {best_gshare['m']}, "
            f"n = {best_gshare['n']} with misprediction rate {format_float(best_gshare['rate'])}%.",
            "",
            "6. In conclusion, with adequate predictor storage budget, gshare rocks.",
            "",
            "---",
            "",
            "## Testing Checklist",
            "",
            "- `./sim bimodal 6 ../test/proj2-traces/gcc_trace.txt`",
            "- `./sim bimodal 9 ../test/proj2-traces/jpeg_trace.txt`",
            "- `./sim bimodal 12 ../test/proj2-traces/perl_trace.txt`",
            "- `./sim gshare 9 3 ../test/proj2-traces/gcc_trace.txt`",
            "- `./sim gshare 15 8 ../test/proj2-traces/gcc_trace.txt`",
            "",
            "To confirm outputs locally, compare against the validation logs:",
            "",
            "```bash",
            "diff -iw <(./sim bimodal 6 ../test/proj2-traces/gcc_trace.txt | sed 's|../test/proj2-traces/||') "
            "../test/proj2-validation/val_bimodal_1.txt",
            "diff -iw <(./sim gshare 9 3 ../test/proj2-traces/gcc_trace.txt | sed 's|../test/proj2-traces/||') "
            "../test/proj2-validation/val_gshare_1.txt",
            "```",
            "",
            "## Rebuilding This Report",
            "",
            "1. From the project root, ensure the simulator is built (`cd c_files && make`).",
            "2. Run `python3 generate_report_assets.py` to regenerate tables, plots, and `report.md`.",
            "3. Convert to PDF, for example with Pandoc: `pandoc report.md -o report.pdf`.",
        ]
    )

    report_path = PROJECT_ROOT / "report.md"
    report_path.write_text("\n".join(markdown_parts) + "\n", encoding="utf-8")


def main() -> None:
    try:
        ensure_environment()
    except FileNotFoundError as exc:
        sys.exit(str(exc))

    print("Gathering bimodal data ...")
    bimodal_df = gather_bimodal_data()
    print("Gathering gshare data ...")
    gshare_df = gather_gshare_data()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    bimodal_csv = OUTPUT_DIR / "bimodal_results.csv"
    gshare_csv = OUTPUT_DIR / "gshare_results.csv"
    bimodal_df.to_csv(bimodal_csv, index=False)
    gshare_df.to_csv(gshare_csv, index=False)
    print(f"Saved bimodal results to {bimodal_csv}")
    print(f"Saved gshare results to {gshare_csv}")

    print("Creating plots ...")
    plot_bimodal_curves(bimodal_df)
    plot_gshare_curves(gshare_df)

    bimodal_summary = build_bimodal_summary(bimodal_df)
    gshare_summary = build_gshare_summary(gshare_df)
    observations = derive_textual_observations(bimodal_summary, gshare_summary)
    best_configs = select_best_configurations(bimodal_df, gshare_df)

    print("Rendering Markdown report ...")
    render_markdown(
        bimodal_df,
        gshare_df,
        bimodal_summary,
        gshare_summary,
        observations,
        best_configs,
    )
    print("Done. See report.md and report_assets/ for outputs.")


if __name__ == "__main__":
    main()
