# Product brief status

Engine name: FactoryCore, chosen by the user. Current version: 0.3.0.

## Implemented prototype scope

- Separate C++20 simulation, command-line runtime/authoring tool, graphics-independent editor session and Windows 3D editor.
- Cylinders, sensors, actuators, motors, conveyors and workpieces with generated visual models, configurable properties, placement, typed inputs/outputs and observable state.
- Deterministic fixed-step simulation, built-in cylinder cycles, latched faults and emergency stop.
- Component creation, deletion, duplication, reusable assemblies and remapped wiring.
- Validated configuration persistence with atomic replacement and relocatable editor asset paths.
- Lua update control, component lifecycle operations and registered prefab spawning.
- GLFW window/input, NVRHI Vulkan rendering, GLM camera/transform math, ImGui controls and an ImGuizmo positioning gizmo.
- glTF/GLB mesh import, PBR materials/textures and text-glTF material export.
- Poly Haven HDRI IBL with diffuse irradiance, roughness-prefiltered specular lighting and a BRDF lookup; four-cascade filtered soft shadows; blurred SSAO; HDR targets and ACES fitted filmic tone mapping.
- Undo/redo, grouped gizmo transactions, play/pause/step/stop, live signal/state inspection and protection for unsaved edits.
- Debug/Release tests, automated UI/GPU checks, dependency hashes/notices, install/ZIP packaging and CI.
- AGENTS.md and a repository simulation skill using Hazel naming.

- Guided editor assembly of an eight-part product cell, cyclic simulated PLC, inspected-product state and per-scan trace.
- Detailed beveled cell assets, 2x viewport supersampling and GPU-downsampled 4K captures; see [product cell](ProductCell.md).

## Qualification and later scope

This is a working visual machine-simulation prototype. Real industrial deployment still requires broader hardware/driver testing, prolonged soak/performance qualification, device-loss recovery, installer/signing work and site-specific acceptance. Current GPU verification uses an Intel Arc 140T; it does not establish compatibility with every Vulkan device.

Production-line scheduling, robot models, external PLC/OPC UA adapters and contact physics remain later extensions, as anticipated by the brief. The current configuration can compose multiple assemblies into a line layout; its idealized equipment behavior does not implement a production-line scheduler.

See [editor usage](Editor.md), [architecture](Architecture.md) and [verification record](Validation.md).
