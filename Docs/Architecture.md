# Architecture

FactoryCoreSimulation owns machine configurations, component identities, equipment state, named signals, fixed ticks and reusable assembly spawning. It has no graphics, operating-system control, scripting-language or networking dependency. Persistence has a small Windows-specific atomic replacement implementation isolated from equipment behavior.

FactoryCoreRuntime advances a machine for an explicit tick budget. An IController applies commands before each tick. CylinderCycleController implements a simulated PLC state machine; FactoryCoreScripting supplies an optional Lua controller. External PLC adapters, including OPC UA, can implement IController later, with transport and thread synchronization kept outside Machine.

FactoryCoreAuthor is the current file-based command-line authoring entry point. The strongly typed Machine API is the structured interface for native tools and agents. Commands validate input and save only after a successful operation. File writes use a sibling temporary file and atomic replacement.

A MachineDefinition is also a reusable assembly. SpawnAssembly validates its components and connections, allocates fresh component IDs, remaps internal connections and applies an offset transactionally. A single configuration can contain several spawned machines and represent a simple production-line layout. There is no specialized production-line scheduler or robot model yet.

The future 3D editor is a separate executable consuming the same API. The requested stack is GLFW for window/input, NVRHI with Vulkan for GPU work and GLM for camera/transform math. VisualModel and Placement store render references and transforms now; the simulation never opens visual assets. Rendering derives animated positions from component state without mutating saved placement.

Graphics work remains an implementation milestone: glTF loading and material validation, PBR, HDR environment lighting, soft shadows, SSAO, HDR render targets and tone mapping, plus a manipulation gizmo. It requires GPU validation, resize/device-loss handling, asset diagnostics and rendered-scene tests before release claims.
