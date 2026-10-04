# FactoryCore development

Read IndustrialSimulationEngine.md for product requirements. Work only in this repository.
The current milestone is a working machine simulation. Read Docs/Status.md before claiming feature completion.

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
