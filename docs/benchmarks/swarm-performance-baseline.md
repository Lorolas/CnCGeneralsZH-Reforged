# Swarm Movement Performance Baseline

This document captures the empirical baseline performance benchmarks for large swarm movement and combat in *Zero Hour Reforged*. These baselines provide concrete targets to evaluate the four proposed performance optimizations:
1. `perf-swarm-throttle` (Time-slicing & avoidance throttling)
2. `perf-swarm-spatial-partition` (Cell presence bitmasks & flat dynamic storage)
3. `perf-swarm-cache-locality` (Sparse active sets & movement cache locality)
4. `perf-swarm-job-parallelism` (Deterministic fork-join steering evaluation)

---

## 1. Test Environment

- **Date**: October 14, 2026
- **Binary**: `generals.exe` (Release build, version 2.8, 64-bit)
- **Host System**: Windows 11 (AMD/Intel Desktop, NVIDIA GeForce RTX 4090, 24 hardware threads)
- **Engine Configuration**: Direct3D 9 headless (`-headless -quickstart -noshellmap -noFPSLimit -observer -multiInstance`)
- **Job Pool**: 23 worker threads (`JobSystem`)

---

## 2. Benchmark 1: Pure Swarm Movement (`mass840`)

### Scenario Description
- **Scenario File**: `Run/Scenarios/mass840.txt`
- **Setup**: 800 combat units (400 Crusader Tanks + 400 Rangers) spawned at Player 1 start and given a single group move order (`playermove 0 * start1`) across a large random map at frame 90.
- **Run Length**: 2,000 logic frames (11.3s wall clock, 176 logic fps, 5.9x real time).
- **Command Line**:
  ```console
  generals.exe -headless -quickstart -noshellmap -observer -multiInstance -noFPSLimit ^
    -randommap 3 2 large -autoskirmish 2 -takeover -seed 3 -maxframes 2000 ^
    -peacetime 10 -scenario mass840 -logPrefix mass840_baseline_
  ```

### Frame-Time Distribution (`HEADLESS LOGICTIME`)

| Percentile / Metric | Time (ms) | Target Budget |
| :--- | :--- | :--- |
| **Mean** | **3.62 ms** | < 1.50 ms |
| **p50 (Median)** | **4.00 ms** | < 1.50 ms |
| **p95** | **4.75 ms** | < 2.50 ms |
| **p99** | **5.25 ms** | < 3.00 ms |
| **p99.9** | **8.25 ms** | < 5.00 ms |
| **Worst Frame** | **9.30 ms** (Frame 34) | < 10.0 ms |
| **Order Frame Spike** | **8.60 ms** (Frame 90) | < 5.0 ms |
| **Frames over 33.3ms** | **0** | 0 |

### Pathfinder & Movement Telemetry (`HEADLESS PATHFIND`)

| Metric | Count | Observations |
| :--- | :--- | :--- |
| **Local Move Checks (`movecheck`)** | **9,632,561** | **~4,900 checks per logic tick** across 800 moving units |
| **Line Passability Checks (`mc.linepass`)** | **8,371,064** | Continuous raycasting ahead of each unit along path |
| **Blocked Unit-Ticks** | **61,790** | Frames where units were stalled behind traffic |
| **Wedged Unit-Ticks** | **98,436** | High friction / jostling among swarm members |
| **Repaths Triggered** | **604** | Full pathfinding searches re-queued due to congestion |
| **Mean Active Objects** | **1,100** (Peak 1,127) | Sustained high unit load |

---

## 3. Benchmark 2: Swarm Combat Clash (`crowdbattle`)

### Scenario Description
- **Scenario File**: `Run/Scenarios/crowdbattle.txt`
- **Setup**: Two mixed armies clashing into each other across the central chokepoints of *Twilight Flame* (4 reinforcement waves of 140 units per side = Crusaders, Rangers, Missile Defenders vs BattleMasters, Red Guards, Tank Hunters).
- **Run Length**: 2,000 logic frames (6.9s wall clock, 291 logic fps, 9.7x real time).
- **Command Line**:
  ```console
  generals.exe -headless -quickstart -noshellmap -observer -multiInstance -noFPSLimit ^
    -map "Maps\Twilight Flame\Twilight Flame.map" -autoskirmish 2 -takeover ^
    -side 0 FactionAmerica -side 1 FactionChina -seed 7 -maxframes 2000 ^
    -scenario crowdbattle -logPrefix crowd_baseline_
  ```

### Frame-Time Distribution (`HEADLESS LOGICTIME`)

| Percentile / Metric | Time (ms) | Target Budget |
| :--- | :--- | :--- |
| **Mean** | **1.42 ms** | < 1.00 ms |
| **p50 (Median)** | **1.50 ms** | < 1.00 ms |
| **p95** | **2.50 ms** | < 1.80 ms |
| **p99** | **3.00 ms** | < 2.20 ms |
| **p99.9** | **12.75 ms** | < 5.00 ms |
| **Worst Frame** | **14.80 ms** (Frame 1831) | < 8.0 ms |
| **Frames over 33.3ms** | **0** | 0 |

### Combat & Pathfinding Telemetry

| Metric | Count | Observations |
| :--- | :--- | :--- |
| **Peak Active Objects** | **753** (Mean 526) | Armies constantly replenished as earlier waves die |
| **Local Move Checks (`movecheck`)** | **2,506,655** | Intense steering around combat wrecks and allies |
| **Line Passability Checks (`mc.linepass`)** | **1,644,556** | Re-checking fire lanes and movement routes |
| **Blocked Unit-Ticks** | **7,815** | Temporary jams in the central valley |
| **Wedged Unit-Ticks** | **9,129** | Traffic at bridge and ramp bottlenecks |

---

## 4. Sub-Frame Profiling Breakdown (`-slowframe 4`)

Analysis of slow transit frames (e.g. Frame 312 of `mass840`) reveals the internal subsystem breakdown:

```
SLOW LOGIC FRAME 312: 4.3ms
  - objects   : 2.8ms  (65%) -> AIUpdateInterface 800x/2.0ms, Physics 663x/0.2ms
  - partition : 0.6ms  (14%) -> 262 queries, 254 gathers, 39,853 candidate objects inspected
  - ai        : 0.9ms  (21%) -> pathfind queue & 9,963 movechecks (6,907 linepass)
  - rest      : 0.0ms  (0%)
```

### Key Subsystem Insights
1. **`objects` Update Loop Dominates**: 70% to 80% of total frame time is spent inside `AIUpdateInterface::update()`.
2. **Heavy Query Volume**: Swarm members execute 260–400 range queries *per frame*, forcing the engine to inspect 40,000 to 60,000 candidate objects *per frame* in `PartitionManager`.
3. **Collision Avoidance Saturation**: Each frame executes ~9,500 to 10,000 `movecheck`s, walking grid cells and checking relationships for units that are moving in open formation.

---

## 5. Optimization Comparison Targets

When implementing the proposals, these are the baseline figures each change aims to improve:

| Proposal | Primary Metric to Beat | Target Improvement |
| :--- | :--- | :--- |
| **`perf-swarm-throttle`** | 9,632,561 `movecheck`s & 61,790 `blocked` ticks | ≥ 40% reduction in `movecheck` calls; drop p95 logic time below 3.0 ms |
| **`perf-swarm-spatial-partition`** | 39,000–60,000 objects examined per frame | ≥ 50% reduction in query object examinations via cell presence bitmasks |
| **`perf-swarm-cache-locality`** | 2.8 ms per-frame `objects` phase | ≥ 20% reduction in `objects` update loop time via dense active sets |
| **`perf-swarm-job-parallelism`** | 14.80 ms peak worst frame / single-thread saturation | Scaling steering evaluations across worker threads; drop p99.9 spike below 6.0 ms |

---

## 6. Optimization Results: `perf-swarm-job-parallelism`

Implementation of deterministic fork-join steering evaluation via `JobSystem::parallel_for`:

### Pure Swarm Movement (`mass840.txt`, 2000 frames)

| Configuration | Wall Clock (s) | Logic FPS | Real-Time Mult | Final CRC | Worker Allocations |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`-jobthreads 0`** (baseline) | 11.7 s | 172 logic fps | 2.9x | `0xD5269569` | 0 |
| **`-jobthreads 4`** | **9.0 s** (-23.1%) | **221 logic fps** (+28.5%) | **3.7x** | `0xD5269569` (bit-identical) | 0 |
| **`-jobthreads 8`** | 9.3 s (-20.5%) | 214 logic fps (+24.4%) | 3.6x | `0xD5269569` (bit-identical) | 0 |

### Swarm Combat Clash (`crowdbattle.txt`, 2000 frames)

| Metric | `-jobthreads 0` | `-jobthreads 4` | `-jobthreads 8` | Improvement (`-jobthreads 4`) |
| :--- | :--- | :--- | :--- | :--- |
| **Wall Clock** | 6.5 s | **6.0 s** | 6.3 s | **-7.7% wall time** |
| **Logic FPS** | 308 fps | **332 fps** | 320 fps | **+7.8% logic fps** |
| **Logic Time (Mean)** | 1.35 ms | **1.05 ms** | 1.14 ms | **-22.2% mean logic time** |
| **Logic Time (p50)** | 1.25 ms | **1.00 ms** | 1.25 ms | **-20.0% median** |
| **Logic Time (p95)** | 2.50 ms | **2.00 ms** | 2.00 ms | **-20.0% p95** |
| **Logic Time (p99)** | 2.75 ms | **2.25 ms** | 2.25 ms | **-18.2% p99** |
| **Logic Time (p99.9)** | 12.50 ms | **8.75 ms** | 12.50 ms | **-30.0% p99.9 spike drop** |
| **Final CRC** | `0x4CD7955D` | `0x4CD7955D` | `0x4CD7955D` | **Bit-for-bit identical** |
| **Worker Allocs** | 0 | 0 | 0 | **Zero allocations** |

---

## 7. Optimization Results: `perf-swarm-spatial-partition`

Implementation of cell presence bitmasks, category filtering, and compact contiguous module traversal in `PartitionManager`:

### Pure Swarm Movement (`mass840.txt`, 2000 frames)

| Metric | Baseline | `perf-swarm-spatial-partition` | Improvement |
| :--- | :--- | :--- | :--- |
| **Mean Logic Time** | 3.62 ms | **2.69 ms** | **-25.7%** |
| **p50 (Median)** | 4.00 ms | **3.00 ms** | **-25.0%** |
| **p95** | 4.75 ms | **3.75 ms** | **-21.1%** |
| **p99** | 5.25 ms | **4.25 ms** | **-19.0%** |
| **Partition Subsystem Time** | 0.60 ms | **0.20–0.30 ms** | **-50.0% to -66.7%** |
| **Frames over 33.3ms** | 0 | **0** | **0** |

### Swarm Combat Clash (`crowdbattle.txt`, 2000 frames)

| Metric | Baseline | `perf-swarm-spatial-partition` | Improvement |
| :--- | :--- | :--- | :--- |
| **Wall Clock** | 6.9 s | **3.7 s** | **-46.4% wall time** |
| **Logic FPS** | 291 fps | **536 fps** | **+84.2% logic fps** |
| **Mean Logic Time** | 1.42 ms | **0.60 ms** | **-57.7% mean logic time** |
| **p50 (Median)** | 1.50 ms | **0.50 ms** | **-66.7% median** |
| **p95** | 2.50 ms | **1.50 ms** | **-40.0% p95** |
| **p99** | 3.00 ms | **2.00 ms** | **-33.3% p99** |
| **p99.9 Spike** | 12.75 ms | **8.75 ms** | **-31.4% p99.9 spike drop** |
| **Final CRC** | `0x4CD7955D` | `0x4CD7955D` | **Bit-for-bit identical** |


