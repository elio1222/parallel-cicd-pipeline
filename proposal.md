# Parallel CI/CD Pipeline Scheduler: Project Proposal

**Course:** CSC475 Parallel Programming and HPC  
**Name:** Elio Rocha  
**Date:** 11-13-2026

I am proposing a parallel CI/CD pipeline runner written in `C++` that uses `OpenMP` threads to automate the building, testing, and packaging of software for developers and their codebase. Hosted CI tools like GitHub Actions and GitLab CI can already run jobs in parallel when dependencies are declared, mostly on separate runners. This project looks at the single-machine case, where every job competes for the same cores and the order jobs start in changes how long the whole pipeline takes. I will test whether scheduling the jobs well, together with a parallel secret scan, can improve speedup and efficiency for developers and teams when they write software and deploy it into production.

As I am in my senior year, I am heading into software engineering, and I want to apply parallel programming to build and CI/CD infrastructure, the field I expect to work in.

---

## 1. The problem and why it matters

CI/CD pipelines finish later than they need to because the work is scheduled poorly, and many pipelines are written as a script that runs one job after another. Developers wait on every pull request, and a pipeline that leaves cores idle wastes both time and money.

The problem is hard because of a few properties:

- **Dependencies.** Jobs form a graph (secret scan, then install, then checks and tests, then packaging), and a job can only start when its prerequisites have finished.
- **Serial scripts.** When jobs run one at a time, cores sit idle while a single job runs, even if other jobs could have started.
- **Uneven durations.** Different jobs take a longer or shorter amount of time than others, so the order the jobs start in changes when the pipeline ends.
- **Late failures.** A quick secret scan that finds a leaked key should stop the pipeline right away, not after minutes of tests and checks.

**What is computed.**

* **The Schedule**: Which jobs run on which thread and when.
* **Secret Scanner**: A data-parallel computation that searches every file in the codebase for API keys and high-entropy strings.

The scheduler decides when the scan and the other jobs run, and the scan itself runs in parallel across files. The scan runs first as a gate: if it finds a secret, the pipeline stops before any other job starts, and if it is clean, the rest of the jobs are scheduled.

**Where parallelism comes in.** The purpose of this project is to create a CI/CD pipeline that does its job faster and in a more convenient way. Since the scheduler chooses which job runs on which thread and when, jobs will be run in parallel using `OpenMP` threads, and each thread runs its job as a separate process. The secret scanner is also parallelized with `OpenMP`, across files and across chunks of large files. As a stretch goal, the hashing step of the scanner will be moved to the GPU using `CUDA` to see whether GPU parallelism helps or whether the cost of copying data cancels it out.

---

## 2. Objectives

1. Build a correct serial baseline: a runner that executes jobs one at a time and a serial secret scanner, each with a reference result to check against.
2. Build a parallel runner on `OpenMP` worker threads that respects dependencies, runs the secret scan first as a gate, and gives the same pass/fail results as the serial run.
3. Build a parallel secret scanner that reports the same findings as the serial scanner.
4. Test at least two optimization strategies, such as scheduling policy and scanner file scheduling, against the baseline.
5. Measure speedup and efficiency at different thread counts.
6. Analyze and explain the results, including where scaling stops, any result that does not improve, and whether or not the parallelized pipeline is worth implementing for codebases.

---

## 3. How it works

**The pipeline.** For the real developer project, the jobs are: the secret scan first, then install, then `ruff`, `mypy`, `bandit`, `pip-audit` and `pytest --cov`, then the coverage gate (it fails the pipeline if test coverage is below a set level), and last `python -m build` to package the project. The runner uses the same idea for synthetic pipelines of made-up jobs.

**The scheduler.** Each job has a counter of unfinished prerequisites. When a counter reaches 0, the job is ready and goes into a shared queue. Every `OpenMP` thread repeats the same loop: take a ready job from the queue, run it as a separate process, mark it finished, and lower the counters of the jobs that were waiting on it. The queue is shared, so access to it is protected with a lock, but the job itself runs without holding the lock.

**The scanner.** The scanner treats each file as a list of bytes and slides a small window of a few bytes across it, one byte at a time. Each window is turned into a number with a hash and looked up in a table of the hashes of known key prefixes, such as `AKIA` for AWS keys and `ghp_` for GitHub tokens. A rolling hash updates the number from the previous window instead of starting over for every window. A match is only a candidate, so the scanner then checks the real bytes and the key length, and long random-looking strings get an entropy check. The whole file is never hashed as one number.

Windows do not depend on each other, so many files, and chunks of one big file, can be scanned at the same time. Large files are cut into chunks that overlap by at least the length of the longest secret, so a key sitting on a chunk boundary is not missed, and each chunk only reports matches that start in its own range. Each thread keeps its own list of findings and the lists are merged once at the end. Real tools such as gitleaks mostly use regular expressions, so this scanner is a simpler, custom version built to be easy to parallelize.

---

## 4. Inputs and outputs

| | Scheduler | Secret scanner |
|---|---|---|
| **Input** | A job file listing each job's command, its prerequisites, and an expected duration, plus a thread count | A folder of files (a repository) and a fixed list of secret patterns |
| **Output** | Pass/fail status for every job, a log of which thread ran which job and when, and the total pipeline time | A list of findings (file, position, kind of secret) and the scan time |

The job file is needed because the prerequisites tell the runner which jobs can run at the same time, and the expected durations let it pick the order (for example longest first) and work out the longest chain of dependent jobs.

The test inputs are synthetic pipelines of made-up jobs and generated repositories from small to large, with and without a planted secret. The exact sizes will be fixed once the baseline runs, so that every run is long enough to measure reliably. The final results will be tables of time, speedup and efficiency per thread count, a plot of speedup against the theoretical bounds, and scanner throughput compared with a measured memory-bandwidth ceiling.

---

## 5. Baseline

The baseline is a plain single-process version of each part of the pipeline. The serial runner will run jobs one at a time in a valid dependency order, with the secret scan first. The serial scanner will loop over the files in sorted order to look for a fixed list of secret patterns.

Every input has a saved reference result: the list of job statuses and the list of findings. The generated repositories have secrets planted at known locations, so the expected findings are known ahead of time. Each job should end with the same exit status in every run, and the parallel version has to report identical findings to the serial version before its time counts.

---

## 6. Optimization strategies

Each strategy is compared against the baseline or against the other options on the same inputs. The first four are the core of the project. The last two are stretch goals if time allows.

| # | Strategy | What changes | Compared against | What I expect |
|---|---|---|---|---|
| 1 | Parallel runner | `OpenMP` threads take ready jobs from a shared queue | The serial runner | Speedup up to about $T_1 / T_{\mathrm{cp}}$, then flat |
| 2 | Scheduling policy | Which ready job goes first: first-come, first-serve, input order, longest/shortest first, or cheap checks first | Each other, on the same pipeline | Longest first lands closest to the lower bound when durations are uneven |
| 3 | Parallel scanner with dynamic scheduling | Files and chunks of large files are handed to threads as they free up, instead of being split evenly in advance | The serial scanner, and static scheduling | Scales until memory bandwidth is the limit |
| 4 | Scan gate placement | Gate first (scan, then everything else) versus speculative (start the other jobs while the scan runs and cancel them if it finds something) | Each other, with and without a planted secret | Gate first stops sooner on failure, speculative finishes sooner on a clean repo |
| 5 (stretch) | SIMD and memory layout in the scanner | Check several bytes at once and lay data out for streaming reads | The scalar scanner | Helps only if the scan is not already limited by bandwidth |
| 6 (stretch) | GPU scanner with `CUDA` | Each GPU thread hashes its own window directly, while verification and entropy checks stay on the CPU | The best CPU scanner, including copy time to and from the GPU | Loses on small inputs because of copies, may win above a break-even size |

---

## 7. Measurement plan

I'll be comparing the parallel version against the serial baseline for correctness, and measure run time, speedup and efficiency on a few input sizes and thread counts.

Measuring the two runners will include measuring the pipeline and scanner time, plus speedup and efficiency at 1, 2, 4, and 8 threads, and comparing them with the theoretical bounds. Comparing scanner throughput with a measured memory-bandwidth ceiling will also be done. The 5600X has 6 cores and 12 threads, so 7 and 8 threads share physical cores, and I expect the speedup to flatten there and will say so in the results. Each job tool will be run with its own threading turned off where possible, so the thread count I report is the number of jobs running at once.

These tests will use synthetic jobs and generated repositories (codebases with software testing) ranging from small to large repos, with and without a planted secret. In addition, a real developer project will be used for the case study to see if this type of tool can be used for my own developer tooling, and possibly be published as open source so that other developers can use this pipeline. Publishing is a bonus and not part of the graded work. To keep the timing stable, the project's dependencies will be downloaded ahead of time so the install job does not depend on the network. Every run uses identical workloads, with warm-up runs and the median of at least 10 repeats, and hardware and compiler flags will be recorded as well.

**Speedup and efficiency**
$$
S_p = \frac{T_1}{T_p}, \qquad E_p = \frac{S_p}{p}
$$

**Bounds on pipeline time**
$$
\max\!\left(T_{\mathrm{cp}}, \frac{T_1}{p}\right) \le T_p \le \frac{T_1}{p} + T_{\mathrm{cp}}
$$

**Fail-fast saving**
$$
T_{\mathrm{saved}} = T_{\mathrm{full}} - T_{\mathrm{fail}}
$$

Here $T_1$ is the time with one thread (the serial baseline), $T_p$ is the time with $p$ threads, and $T_{\mathrm{cp}}$ is the critical path, the longest chain of jobs that depend on each other, which is the time the pipeline would take even with unlimited threads. $T_{\mathrm{full}}$ is the time to run the whole pipeline when nothing fails, and $T_{\mathrm{fail}}$ is the time until the pipeline stops when a secret is planted. With the scan as a gate, $T_{\mathrm{fail}}$ should be close to the scan time.

---

## 8. Hypothesis and expected results

**Hypothesis.**

- With 4 threads and longest-first scheduling, the parallel runner will finish within about 10% of the lower bound $\max(T_{\mathrm{cp}}, T_1/p)$.
- Because every other job waits for the scan, the gate is only cheap once the scanner itself uses several threads.
- The scanner speedup grows with the thread count until it reaches the measured memory-bandwidth ceiling, and then efficiency drops.
- Dynamic scheduling beats static scheduling on repositories where the files are very different in size.

**What would disprove it.**

- The runner is more than about 10% above the lower bound at 4 threads even with longest-first. That would point to lock contention, bad duration estimates, or something I have not modeled, and I would measure where the threads wait.
- The scanner stops scaling well below the bandwidth ceiling. That would point to false sharing, bad chunking or load imbalance instead of a memory limit.
- Static scheduling matches or beats dynamic scheduling. That would mean the files were not uneven enough or that the scheduling overhead dominates, and I would report it that way.

**Results I expect might not improve.**

- **Pipeline speedup flattens early.** Install and the checks waiting on it sit on the critical path, so extra threads sit idle, and the speedup can never go past $T_1 / T_{\mathrm{cp}}$ no matter how many threads are used.
- **The scanner is slower than serial on tiny repos.** Starting threads and merging findings costs more than the scan itself.
- **Scanner efficiency drops once bandwidth is used up.** The scan reads a lot of bytes and does very little math on each one, so cores end up waiting on memory, which is the ceiling the Roofline model describes.
- **7 and 8 threads add little.** They share physical cores on the 5600X.
- **The GPU is slower on small inputs.** Copying data to and from the GPU can cost more than the hashing saves, so I would report the break-even size.
- **The first runs are slower.** The operating system's file cache is cold, which is why warm-up runs and medians are used, and I will also report a cold run.

No minimum speedup is needed for this project to be a success. A result that does not improve is still valid as long as it is correct and I can explain it with the bounds, the bandwidth ceiling, or a measured overhead.

---

## 9. Risks, scope and fallback

- **Scope.** I will build in order: the serial baseline, the parallel runner, the parallel scanner, then the stretch goals. If time runs short, the stretch goals are dropped and the core project still stands.
- **Timing noise.** Other programs on the machine can change the results, so the machine will be kept otherwise idle and every number is a median of at least 10 runs.
- **Real project variability.** Dependencies are downloaded ahead of time to keep install stable. If the real project is still too noisy, the report will rely on the synthetic and generated inputs only.
- **Thread competition.** Job tools that start their own threads can compete with the runner's threads, so they will be limited where possible.
- **GPU risk.** If the `CUDA` setup or kernel does not work out, the report will cover the CPU results only and explain why.

---

## 10. Tools and hardware

The CI/CD serial and parallel runners will be implemented in `C++`, in addition to the scanners. `OpenMP` will be used for the parallelism aspect of this project, utilizing CPU threads, and `CUDA` will be used for the GPU stretch goal. The computer that will run this project includes a `Ryzen 5600x` CPU (6 cores, 12 threads) and a `RTX 3060` GPU. Every reported measurement will come from this one machine, with the operating system, compiler version and flags recorded.

---

## 11. Reproducibility and attribution

- Everything will run from scripts: one to generate the test repositories and pipelines, and one to run all the trials.
- The generated inputs use fixed random seeds, and the raw timings are saved to files, not just the summary tables.
- I will record the CPU, GPU, memory, operating system, compiler and flags, `OpenMP` and `CUDA` versions, and the Python and tool versions (pinned).
- The dependencies of the case-study project are downloaded ahead of time so a rerun does not depend on the network.
- The scheduler and scanner are my own code. `ruff`, `mypy`, `bandit`, `pip-audit`, `pytest`, `coverage` and `build` are run as outside programs and are not rewritten, and the key prefixes (such as `AKIA` and `ghp_`) are publicly documented formats. Any outside code, references or AI assistance will be cited in the final report, following the course's policy.

---

## 12. Deliverables

Source code, the scripts to reproduce the results, performance results, and a final report will be delivered.

---