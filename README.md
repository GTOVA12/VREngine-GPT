# FactoryCore

FactoryCore is a C++20 industrial machine simulation prototype for Windows. This repository implements the first goal in [IndustrialSimulationEngine.md](IndustrialSimulationEngine.md): a small, working machine simulation with reusable equipment, typed signals, control logic and configuration persistence.

The current deliverable is headless. It includes a runtime and command-line authoring tool; the 3D editor and GLFW/NVRHI/Vulkan renderer are not implemented yet. See [feature status](Docs/Status.md) and [architecture](Docs/Architecture.md). Passing prototype tests does not establish production readiness.

## Build and verify on Windows

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.24+, and internet access on the first Lua-enabled configuration. Dependencies are stored under Build; no global package installation is required.

```powershell
cmake --preset windows
cmake --build --preset debug
ctest --preset debug
cmake --build --preset release
ctest --preset release
.\Build\windows\Release\FactoryCoreRuntime.exe --demo
```

Or run the complete build, test, installation and package workflow:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts/Verify.ps1
```

Use `-DFACTORYCORE_ENABLE_LUA=OFF` when configuring a simulation-only build without downloaded dependencies.

## Run a machine

```powershell
.\Build\windows\Release\FactoryCoreRuntime.exe --demo
.\Build\windows\Release\FactoryCoreRuntime.exe Examples/CylinderCell.factory 500 Examples/CylinderCycle.lua
```

The demonstration extends and retracts a cylinder for three complete cycles. The machine file includes a position sensor connected to the cylinder. File runs without a script advance equipment from their initial, uncommanded state.

## Assemble a machine

```powershell
.\Build\windows\Release\FactoryCoreAuthor.exe new Build/MyCell.factory "My cell"
.\Build\windows\Release\FactoryCoreAuthor.exe add Build/MyCell.factory Cylinder "Cylinder"
.\Build\windows\Release\FactoryCoreAuthor.exe add Build/MyCell.factory Sensor "End sensor"
.\Build\windows\Release\FactoryCoreAuthor.exe property Build/MyCell.factory 1 Stroke 0.2
.\Build\windows\Release\FactoryCoreAuthor.exe property Build/MyCell.factory 2 Threshold 0.2
.\Build\windows\Release\FactoryCoreAuthor.exe place Build/MyCell.factory 2 0.2 0 0
.\Build\windows\Release\FactoryCoreAuthor.exe connect Build/MyCell.factory 1 Position 2 Position
.\Build\windows\Release\FactoryCoreAuthor.exe inspect Build/MyCell.factory
```

Commands also support remove, duplicate, disconnect and spawn. To spawn a saved assembly into another machine:

```powershell
.\Build\windows\Release\FactoryCoreAuthor.exe spawn Build/MyCell.factory Examples/CylinderCell.factory 2 0 0
```

IDs printed by add/duplicate/spawn remain stable across save/load. Parameters use metres, seconds, metres per second and radians; motor position and speed use radians and radians per second. Rotation values and visual-model paths are preserved for future rendering.

See [simulation contract](Docs/Simulation.md) for ports, fault semantics and timing, and [scripting](Docs/Scripting.md) for the Lua API.

## Dependency and publication notes

Lua 5.5.1 comes from the official source archive with SHA-256 verification. Its license is included in installed packages. No graphics dependency is fetched by this milestone. The original product brief specifies GLFW, NVRHI, Vulkan and GLM for the future editor.

The intended remote is [GTOVA12/VREngine-GPT](https://github.com/GTOVA12/VREngine-GPT). CI builds and tests Debug and Release on Windows and Linux; Windows is the initial supported development target. CI results should be checked separately from local verification.
