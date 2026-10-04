# Lua scripting

Build with FACTORYCORE_ENABLE_LUA=ON (default). Define OnUpdate(deltaTime, time); the runtime invokes it before every fixed tick. Top-level code defines functions and globals; equipment operations are available only during OnUpdate.

FactoryCore provides:

| Function | Effect |
| --- | --- |
| Create(kind, name) | Create equipment; return a fresh ID |
| Remove(id) | Delete equipment and incident connections |
| Duplicate(id, name) | Duplicate configuration with initial state and a fresh ID |
| SetInput(id, name, value) | Write an unconnected typed input |
| GetSignal(id, name) | Read an output signal |
| Place(id, x, y, z) | Change placement without resetting live state |
| SetProperty(id, name, value) | Validate properties and reset the component's live state |
| SpawnPrefab(name, x, y, z) | Spawn a registered assembly and return an array of IDs |

Kinds are Cylinder, Sensor, Actuator, Motor, Conveyor and Workpiece. Property names are Stroke, Speed, Acceleration, Delay, Threshold, Hysteresis, InitialPosition and Mass. Register named prefabs through ScriptController::RegisterPrefab in the host; the sample runtime does not automatically register machine files as prefabs.

Returned IDs are decimal strings, preserving the full unsigned ID range. Calls also accept positive Lua integer IDs. Boolean inputs require Boolean values; numerical inputs require numerical values.

Every Apply runs against a temporary machine. If an uncaught script error occurs, equipment mutations roll back and the controller rejects future updates. Script globals are not rolled back; construct a new controller to restart. Apply does not itself advance the machine.

Scripts have a configurable instruction budget (100,000 by default), checked every 1,000 instructions. Basic infinite loops fail with a controlled error. Scripts are trusted local code; the instruction hook is not an isolation boundary or a memory quota. The exposed libraries are base, math, table and string; OS, IO, package and file-loading helpers are unavailable. This is a reduced scripting environment rather than a security sandbox.

Lua is compiled as C++ so Lua errors unwind C++ objects in bindings. Engine exceptions are converted to Lua errors at the binding boundary. See Examples/CylinderCycle.lua for a complete controller.
