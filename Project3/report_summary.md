# Project 3 Report Notes

## Validation & Testing
- Run the architectural validation sweep with `./run_validation.sh`; it re-generates `out_val*.txt` and `diff -iw` checks them against the golden outputs.
- Spot-check schedules with the provided scope tool whenever you touch pipeline timing, e.g. `./tool/tool/scope out_val1.txt scope_val1.txt`.
- After code or config changes, rerun the experiment/graph pipeline (`python3 report_experiments.py`) to ensure the report data stays in sync with the simulator.

## Graph & Data Generation
- Command: `python3 report_experiments.py`
  - Runs IQ-size sweeps at ROB=512 for WIDTH ∈ {1,2,4,8} on both traces.
  - Derives the best IQ per width, sweeps ROB sizes {32,64,128,256,512} with that IQ, and logs IPC/Cycle/DIC data.
  - Outputs CSV summaries under `report_data/`, optimal IQ table at `report_data/optimized_iq.csv`, and graphs in `report_data/graphs/`.

Generated graphs:
- `report_data/graphs/gcc_iq_sweep.png`
- `report_data/graphs/gcc_rob_sweep.png`
- `report_data/graphs/perl_iq_sweep.png`
- `report_data/graphs/perl_rob_sweep.png`

## Optimized IQ Size Table
| Trace | WIDTH=1 | WIDTH=2 | WIDTH=4 | WIDTH=8 |
| --- | --- | --- | --- | --- |
| gcc | IQ=16 (IPC 1.00) | IQ=32 (IPC 1.99) | IQ=64 (IPC 3.94) | IQ=128 (IPC 7.73) |
| perl | IQ=16 (IPC 1.00) | IQ=64 (IPC 1.98) | IQ=128 (IPC 3.89) | IQ=256 (IPC 7.55) |

These IQ sizes were chosen as the minimal entries that achieved the max IPC per width at ROB=512.

## ROB Sweep Highlights (using optimized IQs)
- **gcc trace**
  - WIDTH 1: ROB is not the bottleneck; IPC holds at 1.00 from ROB 32 through 512.
  - WIDTH 2: ROB=64 unlocks the full 1.99 IPC; larger ROBs give no extra benefit.
  - WIDTH 4: IPC climbs from 2.38 (ROB 32) to 3.94 (ROB ≥256); ROB>256 is redundant.
  - WIDTH 8: ROB pressure dominates. IPC rises steadily (2.33→3.96→6.18→7.63→7.73) as ROB grows, with diminishing returns beyond ROB 256.
- **perl trace**
  - WIDTH 1 mirrors gcc: ROB size does not matter after 32 entries.
  - WIDTH 2 saturates at ROB ≥128 (IPC 1.98).
  - WIDTH 4 reaches peak IPC 3.89 at ROB ≥256.
  - WIDTH 8 shows the largest sensitivity: IPC improves from 2.18 (ROB 32) up to 7.55 (ROB 512); ROB 512 still buys ~7% over ROB 256 because perl’s longer dependence chains keep more ops in flight.

## Observations for the Report
- IQ pressure dominates at higher widths: both traces roughly double IPC when IQ grows from 8 to the optimal size, before ROB becomes the next limiter.
- Perl needs larger IQs at the same width because its dependency distance is longer; this is visible in the shift of the optimized IQ table.
- ROB scaling shows the pipeline quickly becomes front-end limited for WIDTH≤2, but WIDTH 8 needs both large IQs and large ROBs to approach theoretical IPC.
- Use the CSV and graphs paths above to pull precise numbers/figures into the Word template; all measurements were produced with the validated simulator on the provided traces.

## Report Template Prompts (Verbatim)
NC State University  
Department of Electrical and Computer Engineering  
ECE 463/563 (Prof. Rotenberg)  
Project #3: Dynamic Instruction Scheduling  
REPORT TEMPLATE (Version 1.0)

by

<< YOUR NAME HERE >>

NCSU Honor Pledge: "I have neither given nor received unauthorized aid on this project."

Student’s electronic signature:  ____________________________  
(sign by typing your name)

Course number:  _________________  
(463 or 563 ?)

Grading Breakdown, Experiments, and Report

[0 – 50 points]  Simulator development effort

You may earn up to 50 points for handing in significant commented code for your simulator, even if the simulator does not compile, run, and validate. To receive the maximum of 50 points, there needs to be a good-faith attempt at coding the functionality of all pipeline stages described in Section 5 of the Project 3 specification as well as the supporting code (parsing arguments, reading trace file, producing outputs, the Advance_Cycle() function, the do-while() simulator loop, etc.). Partial credit will be assessed at a coarse granularity based on a qualitative assessment by the TAs of the amount of functionality coded.

[30 points]  Validation

Gradescope will evaluate your simulator on the eight validation runs, “val1.txt” through “val8.txt”, posted on the website. Gradescope will also evaluate your simulator on two mystery runs. Each validation run and mystery run is worth 3 points. Gradescope must say that you match all eight validation runs to get credit for the experiments with the simulator (report), however.

[20 points]  Report: graphs, graph analysis, and discussion

Note: you can only get credit for the report if Gradescope says that you match all eight validation runs.

A. Large ROB, effect of IQ_SIZE

1. Graphs [10 points]: Keep ROB_SIZE fixed at 512 entries so that it is not a resource bottleneck. For each benchmark (gcc and perl), make a graph with IPC on the y-axis and IQ_SIZE on the x-axis. Use IQ_SIZE = 8, 16, 32, 64, 128, and 256. Plot 4 different curves (lines) on the graph: one curve for each of WIDTH = 1, 2, 4, and 8. Title the two graphs “gcc, ROB=512” and “perl, ROB=512”, respectively.

<< INSERT GRAPH “gcc, ROB=512” HERE >>

<< INSERT GRAPH “perl, ROB=512” HERE >>

Grading rubric: (1) 5 points will be deducted for each missing graph. (2) For each graph that exists in the report, the TAs will check for missing or blatantly incorrect datapoints. As there are 24 datapoints (6 iq sizes x 4 widths), the TAs will deduct 0.2 points for each missing or blatantly incorrect datapoint that is spotted.

2. Graph Analysis [2 points]: Using the data in the graphs above, for each WIDTH (1, 2, 4, and 8), find the minimum IQ_SIZE that still achieves within 6% of the IPC of the largest IQ_SIZE (256). This exercise should give four optimized IQ_SIZE’s per benchmark, one optimized for each of WIDTH = 1, 2, 4, and 8. Tabulate the results of this exercise as follows:

|            | “Optimized IQ_SIZE per WIDTH”              |
|            | Minimum IQ_SIZE that still achieves        |
|            | within 6% of the IPC of the largest IQ_SIZE |
|            | gcc                    | perl              |
| WIDTH = 1  |                        |                   |
| WIDTH = 2  |                        |                   |
| WIDTH = 4  |                        |                   |
| WIDTH = 8  |                        |                   |

Grading rubric: Each cell in the table is worth 0.25 points.

3. Discussion [2 points]:

- The goal of a superscalar processor is to achieve an IPC that is close to WIDTH, which is the peak theoretical IPC of the processor. As we increase WIDTH, we observe that a **larger** IQ is needed to achieve this goal. This is because, with greater WIDTH, the IQ needs to look **farther** in the dynamic instruction stream to find **more** independent instructions that can issue in parallel to WIDTH execution lanes, each cycle.
- For WIDTH=8, perl’s “optimized IQ_SIZE” is **greater than** gcc’s “optimized IQ_SIZE” (reference your table above). Why might this be the case?  
  a. Perhaps perl has **more** data-dependent instructions within a fixed window of instructions, such that it **must look farther** in the dynamic instruction stream to get the same number of independent instructions as gcc.  
  b. Perhaps perl has **more** long-latency instructions within a fixed window of instructions as compared to gcc.  
  c. All of the above: both a and b are plausible explanations.  
  Answer: **c**

Grading rubric: Each yellow blank is worth 0.25 points. Note that there are eight yellow blanks.

B. Effect of ROB_SIZE

4. Graphs [6 points]: For each benchmark (gcc and perl), make a graph with IPC on the y-axis and ROB_SIZE on the x-axis. Use ROB_SIZE = 32, 64, 128, 256, and 512. Plot 4 different curves (lines) on the graph: one curve for each of WIDTH = 1, 2, 4, and 8. For a given WIDTH, use the optimized IQ_SIZE for that WIDTH, as obtained from the table in Section A.2 above. Title the two graphs “gcc, optimized IQ_SIZE per WIDTH” and “perl, optimized IQ_SIZE per WIDTH”, respectively.

<< INSERT GRAPH “gcc, optimized IQ_SIZE per WIDTH” HERE >>

<< INSERT GRAPH “perl, optimized IQ_SIZE per WIDTH” HERE >>

Grading rubric: (1) 3 points will be deducted for each missing graph. (2) For each graph that exists in the report, the TAs will check for missing or blatantly incorrect datapoints. As there are 20 datapoints (5 rob sizes x 4 widths), the TAs will deduct 0.15 points for each missing or blatantly incorrect datapoint that is spotted.
