---
name: factorycore-simulation
description: Extend FactoryCore equipment behavior, signal wiring, machine persistence and deterministic simulation tests. Use for this repository's simulation core rather than renderer-only changes.
---

Read Include/FactoryCore/Machine.h and Tests/SimulationTests.cpp before changing the core.
Machine::Step samples all connection sources before updating equipment. Preserve one-tick connection latency and single-driver input ownership.
Keep component IDs stable across save/load. Duplication creates fresh IDs and resets live state without inheriting connections. Removing components clears incident connections and resets affected inputs.
Validate configuration loads and assembly spawning in a temporary machine before replacing a live machine.
For new equipment, define named input/output types, finite parameter bounds, initial state, fault causes and reset behavior. Test these without a graphics context.
Run cmake --preset windows, cmake --build --preset debug and ctest --preset debug; repeat for release before committing. Run the demonstration and installation check.
