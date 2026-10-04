# FactoryCore editor

Build with `cmake --preset editor` and `cmake --build --preset editor-release`. Launch `Build/editor/Release/FactoryCoreEditor.exe`, or add `--machine Examples/CylinderCell.factory` to open the wired cylinder/sensor example.

The default scene contains all six equipment types. Press Play with Cylinder cycle selected to run the first cylinder repeatedly. Pause freezes simulation; Step advances one tick; Stop restores the authored configuration and removes entities spawned during playback. Use Manual control for inspector input commands or select Lua script to run the chosen script. The configured assembly file is registered with Lua as prefab `Assembly`.

For an assembled conveyor/process station controlled by a simulated PLC, see [the FC-01 walkthrough and editor test](ProductCell.md). The --cell-test command builds from an empty scene and exports 4K images and a PLC trace.

## Authoring

- Add equipment in Machine assembly, then select it in the list or viewport. Duplicate and Delete operate on the selection.
- Drag the gizmo to move, rotate or scale. W/E/R select those operations. Choose Local or World space. A completed gizmo drag is one undo operation.
- Right drag orbits, middle drag pans, the wheel zooms and F focuses the selection.
- Edit placement directly in the inspector. Rotation is displayed in degrees; configuration and simulation APIs store radians. Numeric parameters update when their values change.
- Signal wiring selects a source output and destination input. The machine validates types and the single-driver rule before accepting the connection. Connected inputs are read-only in the live inspector.
- Save assembly writes the authored machine as a reusable prefab. Spawn assembly remaps IDs and internal connections.
- The file field in the toolbar selects the save/open path. Save writes the authored configuration even during playback. Open, New and closing the window ask about unsaved changes.
- Input commands, emergency stop, output values, position, velocity, active/fault state and cylinder cycle state are available during playback. Emergency stop takes effect on the next simulation tick.

## Assets and materials

Each equipment type has a generated glTF model with PBR materials and textures. An empty VisualModel uses the packaged default; Import model accepts `.gltf` or `.glb`. Imported models retain their materials and texture references. Scene changes are staged; a failed import leaves the previously rendered scene available and reports the error.

In Visual model and PBR, choose a material, adjust base color, metalness and roughness, enter a new `.gltf` filename, and press Apply and save material. The editor exports a new glTF asset and assigns it to the component. Existing assets are preserved. This material export currently requires a text glTF source; convert a binary GLB to text glTF before editing its materials.

Machine visual references and exported glTF resource URIs are saved relative to their respective files. Machine assets on another Windows drive retain absolute paths; material exports must share a drive with their source resources. Move the machine, models, buffers and textures together while preserving their directory relationships. Default equipment assets are resolved from the installation automatically.

Lighting and display provides SSAO, soft-shadow and HDRI-lighting toggles, AO radius, sun angular size and exposure. Load HDRI accepts an HDR/EXR environment. The bundled environment is Poly Haven's CC0 Studio Small 09. Display settings and camera position are transient; machine files store equipment, placement, properties and wiring.

## Automated verification

```powershell
cmake --preset editor -DFACTORYCORE_GPU_TESTS=ON
cmake --build --preset editor-debug --parallel 6
ctest --preset editor-debug
cmake --build --preset editor-release --parallel 6
ctest --preset editor-release
```

GPU tests need a local Vulkan GPU. VulkanRendering compares rendered images with SSAO, shadows and IBL disabled, checks visible motion, picking, material round trips and resize. EditorUIWorkflow injects real ImGui mouse events and checks editing, undo/redo, playback, save/reload and unsaved-change cancellation/discard. Captures are written to Build/editor/Testing. NVRHI validation errors fail these tests.

`FactoryCoreEditor --smoke --capture output.png` runs the rendering diagnostic without a window. `--frames 4 --capture editor.png` captures a bounded windowed run. `--validation` additionally requests Vulkan validation layers; those layers must be installed separately.

The editor targets Windows and a Vulkan 1.3 driver with Donut/NVRHI's required features. Vulkan SDK installation is unnecessary for a normal build: pinned headers and DXC are downloaded into Build, and compiled shaders are embedded in the executable.
