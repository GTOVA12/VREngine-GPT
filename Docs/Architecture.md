# Architecture

FactoryCoreSimulation owns configurations, component identities, equipment state, named signals, fixed ticks and assembly spawning. It has no graphics, scripting-language or networking dependency. Persistence isolates Windows atomic replacement from equipment behavior.

FactoryCoreRuntime advances machines for an explicit tick budget. An IController applies commands before each tick. CylinderCycleController supplies cylinder cycles; ProductCellController supplies a cyclic input/output-image simulated PLC with sequence interlocks and timeouts; FactoryCoreScripting supplies Lua with transactional tick edits. Future external PLC adapters can implement IController while keeping transport and synchronization outside Machine.

FactoryCoreAuthor exposes file-based commands. The strongly typed Machine API is the structured native interface for tools and agents. Commands validate changes before saving. A MachineDefinition is a reusable assembly: SpawnAssembly allocates fresh IDs, remaps internal wiring and applies placement offsets transactionally.

FactoryCoreEditorModel owns an EditorSession without graphics dependencies. Edits operate on a replacement machine before committing. Undo/redo store configuration snapshots; drag transactions group many placements into one history item. Playback retains a separate authored snapshot and advances fixed simulation ticks from frame elapsed time. Stop restores authoring configuration. EditorFiles saves visual references relative to the machine file and resolves them on loading.

FactoryCoreEditor owns GLFW, NVRHI/Vulkan, GLM, ImGui and ImGuizmo integration. SceneRenderer validates glTF geometry with CPU cgltf parsing, then imports a replacement render scene synchronously. The camera preserves right-handed glTF orientation and converts to Donut positive-forward camera space with mirror tracking. Placement and live equipment state update render transforms without changing saved placement. Topology changes stage replacement scenes and clear obsolete GPU binding caches; texture caches belong to the active scene. Picking tests individual mesh bounds so empty gantry space does not occlude the product. Every shutdown path releases swapchain framebuffers before destroying the Vulkan device, including exception unwinding.

NVIDIA Donut, built on NVRHI, supplies the glTF importer and rendering passes. The pipeline renders cascaded depth shadows, a deferred PBR GBuffer, blurred SSAO, diffuse/specular IBL, environment background and transparent geometry into HDR, followed by an embedded ACES fitted filmic shader into display color and optional 2x supersampling. Quality captures render at twice each output dimension and downsample on the GPU. A neutral backdrop is available while HDRI reflections remain active. Environment changes generate irradiance/specular maps and a BRDF texture once. Resizing rebuilds view-dependent resources after the GPU is idle.

Source archives and the build-time DXC binary have exact revision/hash locks in CMake/Editor.cmake. Vulkan shaders are embedded. The installed editor resolves models, textures, HDRI and example files from share/FactoryCore; runtime asset loading requires no network connection.

Core/session tests run without a window or GPU. Opt-in GPU tests compare feature-on/off captures, material round trips, visible motion and resize. The UI workflow test sends mouse events to real controls and verifies their effects. CI compiles the Windows editor while Linux jobs exercise the portable core with sanitizers.

The Windows prototype reports import errors while retaining the previous scene. Device-loss recovery and a wider hardware qualification matrix remain production-hardening work.
