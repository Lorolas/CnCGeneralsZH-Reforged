# spatial/cell-presence-masks Specification

## Purpose

Maintains compact presence and ownership summaries within the spatial partitioning grid to allow range, target, and collision queries to rapidly bypass non-matching terrain cells without traversing individual entity records.

## Requirements

### Requirement: Spatial Cell Occupancy Tracking
The spatial partitioning system SHALL maintain real-time occupancy summaries for each grid cell, capturing player ownership presence and broad entity categories (such as mobile units, structures, and dynamic obstacles) present within the cell.

#### Scenario: Unit enters a cell
- **WHEN** an entity moves or spawns into a spatial partition cell
- **THEN** the cell's presence summary updates immediately to reflect the entity's player ownership and entity category

#### Scenario: Unit exits a cell
- **WHEN** an entity leaves a spatial partition cell or is destroyed
- **THEN** the cell's presence summary clears that player or category bit if no other entity of that player or category remains in the cell

### Requirement: Cell-Level Query Rejection
Spatial range queries and proximity searches SHALL evaluate cell occupancy summaries against search filter criteria, immediately bypassing any cell that contains no matching entities without inspecting individual entity records.

#### Scenario: Querying for hostile entities in friendly territory
- **WHEN** a unit initiates a search for hostile targets within an area containing cells populated only by friendly or allied entities
- **THEN** the search rejects those cells at the cell-summary stage and examines zero individual candidate entities in those cells

#### Scenario: Querying empty cells in open terrain
- **WHEN** a wide-area spatial query intersects cells that contain no entities
- **THEN** the query skips those empty cells in a single bitmask evaluation without allocating or inspecting candidate intersection records

### Requirement: Deterministic Boundary and Ownership Synchronization
Changes to entity position, footprint coverage, or player affiliation SHALL update cell occupancy summaries synchronously during the logic update phase, ensuring spatial query results remain deterministic and bit-identical across simulation runs.

#### Scenario: Unit changes ownership
- **WHEN** an entity in the spatial grid changes controlling player or alliance status
- **THEN** all partition cells touched by that entity update their player presence masks before any subsequent query is executed in that logic frame

#### Scenario: Replay verification across swarm marches
- **WHEN** identical scenario drills are simulated with cell presence filtering enabled
- **THEN** simulation outcome, unit interactions, and game logic CRC values match bit-for-bit with baseline runs
