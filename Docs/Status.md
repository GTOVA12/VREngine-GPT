# Product brief status

Engine name: FactoryCore, chosen by the user.

## Implemented first milestone

- C++20/CMake simulation library, separate runtime and file-based authoring tool.
- Cylinders, sensors, actuators, motors, conveyors and workpieces with configurable properties, placement, visual-reference metadata, typed inputs/outputs and observable state.
- Deterministic fixed-step simulation, built-in cylinder-cycle control, latched faults and emergency stop.
- Creation, removal, duplication and reusable assembly spawning with internal wiring remapped.
- Saving/loading configurations and composed production-line layouts, strict validation and atomic file replacement.
- Lua OnUpdate control, component lifecycle operations and registered prefab spawning.
- Unit and integration tests, Debug/Release presets, install/ZIP packaging and automated CI.
- AGENTS.md and a repository-local development skill using Hazel naming conventions.

## Remaining product work

- Actual visual models and a 3D editor with a positioning gizmo.
- GLFW/NVRHI/Vulkan and GLM integration.
- glTF mesh/material/texture import and asset management.
- PBR materials, HDRI-based IBL, soft shadow maps, SSAO, HDR rendering and tone mapping.
- Specialized production-line scheduling, robot models and external PLC/OPC UA adapters.
- Scene-scale physics/contact models, persisted live sessions, installer signing and deployment validation in real industrial environments.
- GPU tests, long-running soak tests and performance baselines before production-readiness claims.

The first milestone validates simulation and authoring workflows. It does not fulfill the complete 3D-platform vision.
