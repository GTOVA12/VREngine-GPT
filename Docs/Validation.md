# Local verification - 2026-10-04

Version 0.2.0. Windows x64, Visual Studio 2022, MSVC 19.44.35228, Windows SDK 10.0.26100.0. GPU: Intel Arc 140T, driver 32.0.101.8860.

| Check | Result |
| --- | --- |
| Editor Debug build; project warnings treated as errors | Passed |
| Editor Debug CTest with Lua and GPU tests | 46/46 passed |
| Editor Release build; project warnings treated as errors | Passed |
| Editor Release CTest with Lua and GPU tests | 46/46 passed |
| Graphics-independent Debug and Release suites | 43/43 passed in each |
| Lua-disabled Release suite | 41/41 passed |
| Installed runtime demonstration | Three cycles, 301 ticks, 3.01 seconds |
| Installed authoring tool | Passed |
| Installed renderer using installed assets | Passed |
| Windows editor ZIP | FactoryCore-0.2.0-win64.zip generated |
| Rendered scene and editor captures | Visually inspected |

The GPU suite verifies nonblank output, GLM/Donut transform agreement through picking, visible cylinder motion, PBR material export/import, viewport resize and measurable differences when SSAO, shadow maps or IBL are disabled. On this device, mean byte differences were approximately 0.132 for SSAO, 0.205 for shadows and 29.046 for IBL.

The UI suite injects real mouse events to Add, Duplicate, Delete, Undo, Redo, Play, Pause, Step, Stop, Save, New and Open. It also verifies unsaved-change cancellation/discard. The session suite tests grouped transactions, rollback, fixed timing, overload handling, controller failures, selection and relocating machine/model files.

The core suite covers equipment behavior, typed signals/driver validation, sampled feedback latency, faults/emergency stop, identity/assemblies, malformed files and atomic saves. Lua tests cover lifecycle/prefabs, transactional failure and instruction budgets. Release checks use explicit assertions that remain active.

Review addressed quaternion component ordering, GBuffer previous-view requirements, HDR cubemap depth, numeric ImGui input flags, swapchain color conversion, texture-cache lifetime, unsaved Open/save paths, material value/opacity preservation and atomic material publication.

NVRHI validation is enabled in GPU runs and errors fail automated diagnostics. The optional Khronos Vulkan validation layers were not installed on this machine. CI compiles the Windows editor and runs its non-GPU tests; Linux core jobs use address/undefined-behavior sanitizers. Local passes do not establish all-device compatibility or industrial deployment qualification.
