# Local verification — 2026-10-04

Environment: Windows x64, Visual Studio 2022, MSVC 19.44.35228, Windows SDK 10.0.26100.0.

| Check | Result |
| --- | --- |
| Debug build with warnings treated as errors | Passed |
| Debug CTest, Lua enabled | 42/42 passed |
| Release build with warnings treated as errors | Passed |
| Release CTest, Lua enabled | 42/42 passed |
| Independent Release build with Lua disabled | 40/40 passed |
| Repository skill validation | Passed |
| Staged whitespace/code-style review | Passed |
| Installed runtime demonstration | Three complete cycles, 301 ticks, 3.01 seconds |
| Installed authoring inspection | Passed |
| CPack Windows x64 ZIP | Generated successfully |

Reviewed equipment bounds, signal direction/type/driver validation, sample timing, faults and reset behavior, identity management, transactional assembly/configuration operations, Lua exception boundaries and packaging.

Review fixes are covered by regression checks: placement preserves live state, command controllers reject wired inputs before mutation, and Reset preserves the live session's component-ID high-water mark.

The automated suite covers equipment motion, endpoint clamping, sensor hysteresis, actuator delays, motor acceleration, workpiece transport, faults/emergency stop, connection latency and feedback, deletion/duplication, assembly remapping, malformed/truncated/oversized files, write/read errors, deterministic replay, Lua lifecycle/prefabs/error rollback/instruction limits and command-line authoring.

GitHub CI is configured separately for Windows Debug/Release and a Linux sanitizer build. This local record does not assert remote CI results, GPU validation, industrial deployment qualification or completion of the 3D editor/renderer.
