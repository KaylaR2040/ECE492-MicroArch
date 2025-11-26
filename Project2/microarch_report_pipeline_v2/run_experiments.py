#!/usr/bin/env python3
import itertools, subprocess, yaml, pathlib, json

HERE = pathlib.Path(__file__).parent.resolve()

def load_cfg():
    with open(HERE / "config.yaml","r") as f:
        return yaml.safe_load(f)

def ensure_dir(p: pathlib.Path):
    p.mkdir(parents=True, exist_ok=True)

def iter_sweeps(cfg):
    if "sweeps" in cfg:
        for idx, entry in enumerate(cfg["sweeps"], start=1):
            name = entry.get("name", f"sweep_{idx}")
            grid = entry.get("params") or entry.get("sweep")
            if grid is None:
                grid = {k: v for k, v in entry.items() if k not in {"name", "description"}}
            extra = {
                "traces": entry.get("traces"),
                "description": entry.get("description"),
            }
            yield name, grid, extra
    else:
        grid = cfg.get("sweep")
        if grid is None:
            raise ValueError("config.yaml must define either 'sweep' or 'sweeps'.")
        yield cfg.get("sweep_name", "sweep"), grid, {"traces": None, "description": None}

def main():
    cfg = load_cfg()
    sim_path = (HERE / cfg["sim_path"]).resolve()
    traces = [str((HERE / t).resolve()) for t in cfg["traces"]]
    arg_order = cfg["arg_order"]

    outdir = HERE / "results" / "raw"
    ensure_dir(outdir)

    runs = 0
    for sweep_name, sweep, meta in iter_sweeps(cfg):
        keys = list(sweep.keys())
        values = [sweep[k] for k in keys]
        sweep_runs = 0
        sweep_traces = meta.get("traces") if meta else None
        if sweep_traces:
            trace_list = [str((HERE / t).resolve()) for t in sweep_traces]
        else:
            trace_list = traces

        for combo in itertools.product(*values):
            params = dict(zip(keys, combo))

            # Resolve FA for L1_ASSOC (assoc = number of lines = size_bytes / block_bytes)
            bs = int(params["BLOCKSIZE"])
            l1_size = int(params["L1_SIZE"])
            assoc = params["L1_ASSOC"]
            if isinstance(assoc, str) and assoc.upper() == "FA":
                assoc = max(1, l1_size // bs)
            params["L1_ASSOC"] = int(assoc)

            for trace in trace_list:
                params["TRACE"] = trace

                # Build argv in the specified order
                argv = [str(params[k]) for k in arg_order]
                cmd = [str(sim_path)] + argv
                print(f"[{sweep_name}] Running:", " ".join(cmd))

                # Run the sim, capture stdout
                res = subprocess.run(cmd, capture_output=True, text=True)
                runs += 1
                sweep_runs += 1

                # Save raw output
                tag = (
                    f"{sweep_name}__"
                    f"blk{params['BLOCKSIZE']}_"
                    f"L1{l1_size}_A{params['L1_ASSOC']}_"
                    f"L2{params['L2_SIZE']}_A{params['L2_ASSOC']}_"
                    f"N{params['PREF_N']}_M{params['PREF_M']}"
                )
                base = pathlib.Path(trace).name.replace(".","_")
                rawfile = outdir / f"{base}__{tag}.txt"
                with open(rawfile, "w") as f:
                    f.write(res.stdout)
                    if res.stderr:
                        f.write("\n--- STDERR ---\n")
                        f.write(res.stderr)

                # Persist the parameters alongside stdout so the parser can recover them
                paramsfile = rawfile.with_suffix(".json")
                param_snapshot = {k: v for k, v in params.items()}
                param_snapshot["TRACE"] = trace
                param_snapshot["sweep"] = sweep_name
                paramsfile.write_text(json.dumps(param_snapshot, indent=2))

                if res.returncode != 0:
                    print(f"[WARN] Simulator exited with code {res.returncode}: {' '.join(cmd)}")

        print(f"[{sweep_name}] Completed {sweep_runs} runs.")

    print(f"All sweeps complete. Total runs: {runs}. Now parse with: python3 parse_results.py")

if __name__ == "__main__":
    main()
