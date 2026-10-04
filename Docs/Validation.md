# Local verification - 2026-10-04

Version 0.3.0. Windows x64, Visual Studio 2022, MSVC 19.44.35228, Windows SDK 10.0.26100.0. GPU: Intel Arc 140T, driver 32.0.101.8860.

| Check | Result |
| --- | --- |
| Editor Debug build; project warnings treated as errors | Passed |
| Editor Debug CTest with Lua and GPU tests | 49/49 passed |
| Editor Release build; project warnings treated as errors | Passed |
| Editor Release CTest with Lua and GPU tests | 49/49 passed |
| Graphics-independent Debug and Release suites | 44/44 passed in each |
| Lua-disabled Release suite | 42/42 passed |
| Installed runtime demonstration | Three cycles, 301 ticks, 3.01 seconds |
| Installed authoring tool | Passed |
| Installed renderer using installed assets | Passed |
| Installed editor assembly and simulated PLC cycle | Eight parts, five wires, one product, 1,220 scans / 12.20 seconds |
| Windows editor ZIP | FactoryCore-0.3.0-win64.zip generated |
| Five actual 3840 x 2160 renders and editor captures | Visually inspected |

Reproduce the full local workflow with `Scripts/Verify.ps1 -GpuTests`. Core-only verification uses `Scripts/Verify.ps1 -CoreOnly`.

The GPU suite verifies nonblank output, GLM/Donut transform agreement through picking, visible cylinder motion, PBR material export/import, viewport resize and measurable feature-on/off image differences. Installed-renderer mean byte differences were approximately 47.471 for ACES filmic tone mapping, 0.158 for SSAO, 1.037 for shadows and 61.077 for HDRI lighting. These checks establish that the passes affect output; visual inspection evaluates the resulting presentation.

The existing UI suite injects real mouse events to Add, Duplicate, Delete, Undo, Redo, Play, Pause, Step, Stop, Save, New and Open, including unsaved-change cancellation/discard. ProductCellEditor starts empty, installs all eight parts through the editor, verifies their full definitions, connects five signals, saves/reloads, selects the simulated PLC, plays through processing/discharge and verifies authored restoration after Stop. It records every PLC scan and captures the assembled, held and inspected product. The five quality images render at 7680 x 4320 before GPU downsampling.

ProductCellPLC covers all eleven normal sequence states, deterministic replay, a stationary product during processing, clamp/head/discharge interlocks, emergency stop during three phases, equipment faults, explicit recovery, a missing-sensor timeout, invalid sensor ordering, missing/conflicting wiring and transactional output rollback. Processed state latches during a run and is excluded from configuration saves.

Core/session tests cover equipment behavior, typed signals, sampled one-tick connection latency, faults/emergency stop, identity/assemblies, malformed files, atomic saves, grouped editor transactions, rollback, fixed timing, controller failures and file relocation. Lua tests cover lifecycle/prefabs, transactional failure and instruction budgets. Release checks remain active.

Review additionally corrected exception cleanup ordering so swapchain framebuffers are released before the Vulkan device, including failed initialization. VulkanFailureCleanup exercises that error path. Per-mesh picking fixes selection through empty gantry space, and ProductCellEditor checks picking with viewport supersampling. Model validation parses glTF on the CPU; staged scene replacement loads synchronously and clears binding caches before publication. Asset/source hashes and installed license notices were checked.

NVRHI validation is enabled in GPU runs and errors fail automated diagnostics. The optional Khronos Vulkan validation layers were not installed on this machine. CI compiles the Windows editor and runs its non-GPU tests; Linux core jobs use address/undefined-behavior sanitizers. The PLC is simulated; these results do not qualify a physical controller, contact physics or every Vulkan device.
