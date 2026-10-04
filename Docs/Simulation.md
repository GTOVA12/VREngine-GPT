# Simulation contract

Machine is a single-owner, single-threaded API. Apply edits or controller commands between Step calls. Its fixed time step is finite and lies between 1 microsecond and 1 second. IDs are positive unsigned 64-bit values; zero and the maximum value are reserved. A machine contains at most 10,000 components.

Each Step samples every wired output before changing any input or component state, applies the sampled values, then updates equipment in ascending ID order. Wiring has one tick of latency, including feedback loops. Different insertion orders therefore produce the same equipment behavior. Every input has at most one wired source. An external SetInput call on a wired input is rejected; disconnecting or deleting the source resets that input to false or zero.

All components have a Boolean Fault input and Fault/Active Boolean outputs plus Position/Velocity numeric outputs. Every property and transform must contain finite, bounded values. Geometry scales, stroke, speed, acceleration and mass must be positive. Delay and hysteresis cannot be negative.

| Equipment | Additional inputs | Additional outputs | Behavior |
| --- | --- | --- | --- |
| Cylinder | Extend, Retract (bool) | Extended, Retracted (bool) | Constant-speed travel clamped to [0, Stroke]; no command holds position |
| Sensor | Position (number) | Detected (bool) | Threshold comparison with optional hysteresis |
| Actuator | Enable (bool) | — | Symmetric configured on/off delay; interrupted transitions cancel |
| Motor | Enable, Reverse (bool) | — | Speed target with acceleration/deceleration limit; integrates angular position |
| Conveyor | Enable, Reverse (bool) | — | Speed target with acceleration/deceleration limit; integrates travel |
| Workpiece | Enable, Processed (bool), Velocity (number) | Processed (bool) | Integrates commanded velocity while enabled |

Workpieces are enabled on creation. Sensor initial input is InitialPosition. Other command inputs start false or zero. These models are kinematic approximations: no collision solver, pneumatic pressure, friction, force coupling or physical workpiece contact is modeled. Connect Conveyor.Velocity to Workpiece.Velocity for transport. Processed latches an inspection flag until Reset; the Processed Boolean output exposes it. The flag is live state and is excluded from configuration saves. See ProductCell.md for the cyclic simulated PLC example.

An external Fault input or opposing cylinder commands latches a fault and stops motion. Fault causes must be cleared before ResetFault; cylinder commands must both be false. Faults do not clear automatically. Exceeding the simulation coordinate bound also latches an external fault. Emergency stop stops motion on the next Step without resetting position. Clearing emergency stop permits motion from current commands; controllers must implement their own restart policy. CylinderCycleController enters Faulted after emergency stop and requires explicit Start.

Place changes saved placement without changing inputs or simulation state. Configure changes validated properties and resets that component's live state and inputs. Duplicate resets state, allocates a fresh ID and inherits no connections. Reset reconstructs all initial state, clears commands/faults/emergency stop and resets time to zero.

Configuration files start with FACTORYCORE 1 and store the name, fixed time step, IDs, equipment kinds, visual references, placements, properties and connections. They do not store live input values, faults, emergency stop, controller state or tick counters. Loading validates all components and wiring in a temporary machine before replacement. Strings are quoted and escaped; numerical output uses round-trip precision and the classic locale. Files are limited to 8 MiB and reject unsupported versions, unknown kinds/ports, duplicate IDs/drivers and trailing data.

Atomic replacement protects an existing file from ordinary write failures. It is not a transaction across multiple files or a guarantee against power loss on every filesystem.
