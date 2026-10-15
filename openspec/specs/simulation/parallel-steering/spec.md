# Parallel Steering Specification

## Purpose

Enables deterministic multi-threaded evaluation of swarm steering, avoidance, and passability computations without desynchronizing lockstep simulation state.

## Requirements

### Requirement: Deterministic Multi-Threaded Simulation Consistency
The simulation system SHALL produce bit-for-bit identical logic state, unit trajectories, and frame checksums (CRCs) regardless of the number of worker threads allocated for parallel steering evaluation.

#### Scenario: Multi-threaded execution matches single-threaded baseline
- **WHEN** a swarm movement scenario is executed with multiple worker threads enabled
- **THEN** every frame logic CRC and final unit position matches the execution with zero worker threads bit-for-bit

#### Scenario: Replay verification across varying thread counts
- **WHEN** a recorded match replay is played back under differing worker thread counts (including 0, 2, 4, and 8 threads)
- **THEN** playback completes without desynchronization, CRC mismatches, or divergence in match outcomes

### Requirement: Order-Independent Output Commit
The simulation system SHALL apply steering and movement evaluation results in a strictly deterministic, predetermined entity sequence irrespective of worker thread execution scheduling or completion order.

#### Scenario: Workers complete out of order
- **WHEN** parallel worker threads finish steering computations for unit batches in non-sequential order
- **THEN** computed velocities, headings, and positions are committed in ascending unique entity identifier order, maintaining deterministic simulation sequence

### Requirement: Zero-Allocation Parallel Execution
Parallel steering evaluation tasks executed across worker threads MUST NOT perform dynamic heap or pool memory allocations during the parallel execution phase.

#### Scenario: Swarm movement under worker execution
- **WHEN** parallel worker threads evaluate path passability, obstacle distances, and crowd avoidance for moving entities
- **THEN** the worker allocation counter remains at zero throughout the entire parallel evaluation phase

### Requirement: Single-Threaded Fallback
The simulation system SHALL support synchronous inline execution of steering evaluation when zero worker threads are configured or when running on single-core platforms.

#### Scenario: Zero worker thread configuration
- **WHEN** the engine is initialized with worker threads disabled or zero worker threads specified
- **THEN** steering computations execute sequentially on the calling simulation thread without functional divergence or runtime error
