# FactoryCore development

Read IndustrialSimulationEngine.md for product requirements. Work only in this repository.
The current deliverable is a working visual machine simulation with a separate core/runtime/editor. Read Docs/Status.md before claiming feature completion.

- Use C++20 and CMake. Match Hazel naming: PascalCase types/functions/files/namespaces, camelCase locals/parameters, m_PascalCase private members, s_PascalCase static variables. Use tabs and Allman braces in C++.
- Keep simulation independent of GLFW, NVRHI, Vulkan, GLM, Lua and editor state. Graphics and scripting consume validated simulation interfaces.
- Preserve deterministic fixed-step execution and ordered component IDs. Never read wall-clock time from the core.
- Sample every connection before updating equipment. Connections have one tick of latency and one driver per input; keep tests and documentation consistent.
- Validate finite numbers, property ranges, identity uniqueness and signal types before mutation. Configuration loads and assembly spawning must be transactional.
- Faults latch and stop equipment motion. Clear fault causes and commands before explicit reset. Emergency stop stops all motion on the next tick.
- Save configuration rather than live cycle state. Change the file format version when incompatible changes are necessary.
- Test normal operation, fault recovery, boundary conditions and malformed files. Test checks must remain active in Release.
- Before committing, review the diff, build and run CTest in Debug and Release, run the sample runtime, and check installation. Never publish untested code as production-ready.
- Inspect remote history before publishing; never force-push.
- Use .agents/skills/factorycore-simulation for equipment, connections or persistence changes.

- EditorSession owns authored configuration, undo transactions and fixed-step playback. Stop restores the authored machine; save must never persist runtime-spawned objects or live inputs.
- Save editor visual references relative to machine files, and keep glTF resource URIs relative to their asset files. Preserve imported material opacity and source assets when exporting changes.
- Use GLM column-major camera matrices consistently with Donut row-major math. Donut quaternions take w,x,y,z; GLM fields are named explicitly. Keep picking and gizmo projection tests aligned with rendering.
- Own GPU resources through NVRHI handles. Stage scene replacement, wait for safe GPU use before releasing resources, and clear raw-pointer binding caches when replacing scenes. Release swapchain framebuffers before the Vulkan device on every shutdown path, including exceptions.
- Pin all external archives and build tools to exact revisions and verified hashes. Ship their complete license notices and asset attribution.
- Renderer changes require editor Debug/Release builds and local GPU/UI tests where hardware is available. Run Scripts/Verify.ps1 -GpuTests for the full Windows workflow; keep CI independent of GPU availability.
- Keep cyclic PLC input/output images separate from equipment updates. Test interlocks, timers, latched faults, deterministic replay and interruption during motion/processing/discharge.
- Validate model data on the CPU. Scene replacement loads synchronously before GPU publication; do not create throwaway GPU scenes for asset validation.
- Preserve right-handed glTF world orientation and positive-forward Donut camera space with a reflected view that PlanarView tracks. Test picking after viewport supersampling and inspect text orientation in captures.
- ProductCellEditor must start empty and build/wire through real editor mouse events. Render evidence must come from the running scene, including a held product and completed discharge.
