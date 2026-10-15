# simulation/sparse-active-sets Specification

## Purpose

Defines the lifecycle management, determinism invariants, and iteration contracts for compact sparse active object collections and cache-friendly active movement in the simulation engine.

## Requirements

### Requirement: Deterministic Lifecycle of Sparse Active Object Sets
The simulation engine SHALL maintain compact sparse active collections for entities experiencing transient lifecycle conditions (including disabled, EMP-shocked, or recovering states) rather than traversing every world entity each tick. Membership in sparse active sets SHALL be registered immediately upon entering the qualifying condition and deregistered immediately upon returning to default operational status.

#### Scenario: Object enters disabled condition
- **WHEN** a simulation entity transitions from an active operational state to a disabled or EMP state
- **THEN** the entity is registered in the sparse disabled active set without altering the relative ordering of existing members

#### Scenario: Object recovers from disabled condition
- **WHEN** the disabled timer expires during simulation stepping
- **THEN** the entity clears its disabled condition and is deregistered from the active set without leaving stale entries

### Requirement: Deterministic Iteration Order Across Clients
The simulation engine SHALL iterate sparse active collections in a strictly deterministic sequence invariant to memory addresses, allocation ordering, or client platform differences. Stepping sparse sets SHALL produce bit-identical simulation results and synchronization hashes across all networked clients and replay runs.

#### Scenario: Replay consistency across multi-instance runs
- **WHEN** a headless replay or multiplayer scenario is executed across different runs or client platforms
- **THEN** the traversal order of sparse active sets yields identical state mutations and frame synchronization checksums

### Requirement: Zero Overhead for Inactive or Quiescent Entities
The simulation loop SHALL NOT inspect, query, or evaluate disabled status timers for entities that are currently enabled and operating normally. The processing time dedicated to rare transient states SHALL scale strictly with the count of actively affected entities rather than the total world entity count.

#### Scenario: Massive army with zero disabled units
- **WHEN** a scenario containing hundreds of moving and idle units executes with no active disabled effects
- **THEN** the simulation bypasses disabled-state status evaluations entirely for all unaffected units in the frame

### Requirement: Immediate Consistency on Entity Destruction
When an entity participating in any sparse active set is destroyed, sold, or removed from the simulation, the system SHALL immediately detach and invalidate its entry from all sparse active collections before subsequent ticks or subsystem queries execute.

#### Scenario: Destruction of an active-set entity
- **WHEN** an entity currently registered in a sparse active set is destroyed during combat or script execution
- **THEN** the entity is purged from the active set immediately, preventing dangling references or zombie updates in subsequent frame phases

### Requirement: Contiguous Memory Locality for Active Movement Properties
The simulation engine SHALL maintain hot kinematic movement properties for active moving units in contiguous memory buffers during transit to ensure predictable cache locality. Units that transition to stationary or idle states SHALL NOT pollute the contiguous active mover memory footprint.

#### Scenario: Large swarm movement command
- **WHEN** a swarm of hundreds of units is issued a transit order across the map
- **THEN** the simulation reads and updates the kinematics of moving units sequentially through contiguous buffers without chasing scattered pointers

#### Scenario: Unit arrives and transitions to stationary state
- **WHEN** a unit reaches its destination and completes its move order
- **THEN** its kinematic data is removed from the active mover buffer and returned to quiescent storage
