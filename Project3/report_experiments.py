#!/usr/bin/env python3
"""
Run Project 3 experiments, collect IPC results, and generate graphs for the report.
"""
from __future__ import annotations

import csv
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parent
SIM_PATH = ROOT / "sim"
TRACE_CONFIG = {
    "gcc": ROOT / "proj3-traces/val_trace_gcc1",
    "perl": ROOT / "proj3-traces/val_trace_perl1",
}
IQ_SIZES = [8, 16, 32, 64, 128, 256]
WIDTHS = [1, 2, 4, 8]
ROB_SWEEP = [32, 64, 128, 256, 512]
ROB_FOR_IQ_SWEEP = 512

RESULTS_DIR = ROOT / "report_data"
GRAPHS_DIR = RESULTS_DIR / "graphs"
CSV_PATH = RESULTS_DIR / "results.csv"
OPT_IQ_PATH = RESULTS_DIR / "optimized_iq.csv"


@dataclass
class SimResult:
    trace: str
    width: int
    iq: int
    rob: int
    dic: int
    cycles: int
    ipc: float
    sweep: str  # "iq" or "rob"


def run_sim(rob: int, iq: int, width: int, trace: Path) -> Tuple[int, int, float]:
    cmd = [str(SIM_PATH), str(rob), str(iq), str(width), str(trace)]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    dic = cycles = None
    ipc = None
    for line in proc.stdout.splitlines():
        if line.startswith("# Dynamic Instruction Count"):
            dic = int(line.split("=")[1].strip())
        elif line.startswith("# Cycles"):
            cycles = int(line.split("=")[1].strip())
        elif line.startswith("# Instructions Per Cycle"):
            ipc = float(line.split("=")[1].strip())
    if dic is None or cycles is None or ipc is None:
        raise RuntimeError(f"Failed to parse sim output for command: {' '.join(cmd)}")
    return dic, cycles, ipc


def ensure_paths() -> None:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    GRAPHS_DIR.mkdir(parents=True, exist_ok=True)


def write_csv(results: List[SimResult]) -> None:
    with CSV_PATH.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            ["trace", "sweep", "width", "iq_size", "rob_size", "dynamic_inst", "cycles", "ipc"]
        )
        for r in results:
            writer.writerow([r.trace, r.sweep, r.width, r.iq, r.rob, r.dic, r.cycles, f"{r.ipc:.6f}"])


def write_optimized_table(best_iq: Dict[str, Dict[int, SimResult]]) -> None:
    with OPT_IQ_PATH.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["trace", "width", "optimal_iq_size", "ipc"])
        for trace, data in best_iq.items():
            for width in WIDTHS:
                best = data[width]
                writer.writerow([trace, width, best.iq, f"{best.ipc:.6f}"])


def plot_sweep(trace: str, sweep: str, series: Dict[int, List[Tuple[int, float]]], xlabel: str, filename: Path) -> None:
    plt.figure(figsize=(7, 4))
    for width in WIDTHS:
        points = sorted(series[width], key=lambda p: p[0])
        if not points:
            continue
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        plt.plot(xs, ys, marker="o", label=f"WIDTH={width}")
    plt.xlabel(xlabel)
    plt.ylabel("IPC")
    plt.title(f"{trace.upper()} {sweep.upper()} Sweep")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()
    plt.savefig(filename)
    plt.close()


def main() -> int:
    if not SIM_PATH.exists():
        print(f"Simulator not found at {SIM_PATH}. Build it before running experiments.", file=sys.stderr)
        return 1
    ensure_paths()
    all_results: List[SimResult] = []
    best_iq_table: Dict[str, Dict[int, SimResult]] = {}

    for trace_label, trace_path in TRACE_CONFIG.items():
        print(f"=== Trace: {trace_label} ({trace_path}) ===")
        best_for_trace: Dict[int, SimResult] = {}
        iq_series: Dict[int, List[Tuple[int, float]]] = {w: [] for w in WIDTHS}
        rob_series: Dict[int, List[Tuple[int, float]]] = {w: [] for w in WIDTHS}

        for width in WIDTHS:
            best_result: Optional[SimResult] = None
            for iq in IQ_SIZES:
                dic, cycles, ipc = run_sim(ROB_FOR_IQ_SWEEP, iq, width, trace_path)
                result = SimResult(trace_label, width, iq, ROB_FOR_IQ_SWEEP, dic, cycles, ipc, "iq")
                all_results.append(result)
                iq_series[width].append((iq, ipc))
                if best_result is None or ipc > best_result.ipc or (
                    abs(ipc - best_result.ipc) < 1e-9 and iq < best_result.iq
                ):
                    best_result = result
                print(
                    f"  IQ sweep width={width} iq={iq}: IPC={ipc:.4f} (cycles={cycles}, instr={dic})"
                )
            assert best_result is not None
            best_for_trace[width] = best_result
            print(
                f"  -> Best WIDTH {width}: IQ {best_result.iq} gives IPC {best_result.ipc:.4f}"
            )

        best_iq_table[trace_label] = best_for_trace

        for width in WIDTHS:
            iq_choice = best_for_trace[width].iq
            for rob in ROB_SWEEP:
                dic, cycles, ipc = run_sim(rob, iq_choice, width, trace_path)
                result = SimResult(trace_label, width, iq_choice, rob, dic, cycles, ipc, "rob")
                all_results.append(result)
                rob_series[width].append((rob, ipc))
                print(
                    f"  ROB sweep width={width} rob={rob} (iq={iq_choice}): IPC={ipc:.4f} "
                    f"(cycles={cycles}, instr={dic})"
                )

        plot_sweep(
            trace_label,
            "iq",
            iq_series,
            "IQ Size",
            GRAPHS_DIR / f"{trace_label}_iq_sweep.png",
        )
        plot_sweep(
            trace_label,
            "rob",
            rob_series,
            "ROB Size",
            GRAPHS_DIR / f"{trace_label}_rob_sweep.png",
        )

    write_csv(all_results)
    write_optimized_table(best_iq_table)
    print(f"\nWrote detailed results to {CSV_PATH}")
    print(f"Wrote optimal IQ table to {OPT_IQ_PATH}")
    print(f"Graphs saved under {GRAPHS_DIR}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
