# FactoryCore

FactoryCore is a C++20 industrial machine simulation prototype with a Windows 3D editor, a separate runtime, reusable equipment, typed signals, built-in/Lua control and configuration persistence.

Version 0.2.0 includes GLFW/NVRHI Vulkan rendering, GLM, glTF models/materials/textures, positioning gizmos, PBR, HDRI lighting, soft shadows, SSAO, HDR and tone mapping. See [editor usage](Docs/Editor.md), [feature status](Docs/Status.md) and [architecture](Docs/Architecture.md).

## Build and launch the editor

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.25+, and a Vulkan 1.3 GPU/driver supporting NVRHI's required features. First configuration downloads pinned dependencies into Build. No global dependency installation or Vulkan SDK is required.

```powershell
cmake --preset editor
cmake --build --preset editor-release --parallel 6
ctest --preset editor-release
.\Build\editor\Release\FactoryCoreEditor.exe --machine Examples/CylinderCell.factory
```

For the full Debug/Release, GPU/UI test, installation and package workflow:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts/Verify.ps1 -GpuTests
```

The GPU checks require a local graphics device. Hosted CI compiles the editor and runs the non-GPU suite. The current prototype has passed local tests on Intel Arc 140T; broader driver testing and industrial deployment acceptance remain necessary. See [validation](Docs/Validation.md).

## Build and verify on Windows

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.25+, and internet access on the first Lua-enabled configuration. Dependencies are stored under Build; no global package installation is required.

```powershell
cmake --preset windows
cmake --build --preset debug
ctest --preset debug
cmake --build --preset release
ctest --preset release
.\Build\windows\Release\FactoryCoreRuntime.exe --demo
```

For the graphics-independent workflow:

```powershell
powershell -ExecutionPolicy Bypass -File Scripts/Verify.ps1 -CoreOnly
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

IDs printed by add/duplicate/spawn remain stable across save/load. Parameters use metres, seconds, metres per second and radians; motor position and speed use radians and radians per second. Rotation values and visual-model paths are consumed by the editor.

See [simulation contract](Docs/Simulation.md) for ports, fault semantics and timing, and [scripting](Docs/Scripting.md) for the Lua API.

## Dependency and publication notes

Dependencies use official source archives and SHA-256 verification. Graphics builds additionally pin Donut, NVRHI, GLFW, GLM, ImGui, ImGuizmo and DXC. Shader binaries, default models/textures and the CC0 HDRI are packaged for offline runtime use. Full dependency notices accompany installed packages.

The intended remote is [GTOVA12/VREngine-GPT](https://github.com/GTOVA12/VREngine-GPT). CI builds and tests Debug and Release on Windows and Linux; Windows is the initial supported development target. CI results should be checked separately from local verification.
