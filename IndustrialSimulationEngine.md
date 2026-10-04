# AI Game Engine

We're building a production-grade, simple and straightforward 3D industrial shop floor simulation platform for Windows. This is professional-grade software that needs to be stable, fully tested, and deployable in real-world environments. No hacks or shortcuts: solid as a rock.
Users should be able to assemble machines from reusable components, simulate their behavior, and connect them into production lines.  
The first goal is a small, working machine simulation. The platform can later grow to support complete production lines, robots, and connections to external PLCs. The software should be reliable, testable, and deployable. The initial prototype should focus on validating the simulation workflow before targeting every operating system.

## Development workflow

- Ask questions only when absolutely necessary; work autonomously.
- Start by asking a question to determine the engine name. Suggest a few options.
- Don't look into other folder
- Testing is of the utmost importance: use lots of unit tests (any framework is fine, even a simple one) and run automated testing.
- Do things properly. This is a production-grade project, not a hack project.
- Create an `AGENTS.md` and relevant skills to support development, with concrete development guidelines.
- Match the code style of [Hazel](https://docs.hazelengine.com/HazelForEngineers/DeveloperGuide#naming). The full repository is [here](https://github.com/TheCherno/Hazel).
- Create a Git repository, commit, and push to the [GitHub repository](https://github.com/GTOVA12/VREngine-GPT). **Before committing**, perform a code review and make sure all changes comply with the code style, meet production-grade quality standards, and have been properly tested. Add unit tests where necessary and make sure they pass.

## Tech stack

- C++ and CMake for the core engine.
- [GLFW](https://github.com/glfw/glfw) and [NVRHI](https://github.com/NVIDIA-RTX/NVRHI), using Vulkan primarily on all platforms.
- [GLM](https://github.com/g-truc/glm) for math.
- Lua for scripting, though this can be discussed.

## Basic architecture

- Separate the core simulation, the 3D editor, and the runtime.
- Represent machines as reusable assemblies made of equipment components.
- Give each component a visual model, configurable properties, a simulation model, and input/output signals.
- Provide a structured interface that allows users and AI agents to create machines, place components, edit parameters, and connect signals.
- Keep the simulation model independent from the rendering code so it can be tested without opening the editor.
- Support saving and loading machine and production-line configurations.

## Industrial simulation scope
- Users can assemble a machine from reusable equipment components.
- Initial components include cylinders, sensors, actuators, motors, conveyors, and workpieces.
- Each component has a visual representation, configurable properties, a simulation behavior, and input/output signals.
- Components can be connected to a control logic.
- The simulation shows component states such as position, active/inactive, cycle state, and fault state.
- Machines can later be connected to form production lines.

## Equipment behavior and control
- Simulate basic equipment behavior, starting with cylinders, sensors, actuators, and workpieces.
- For example, a cylinder receives extend and retract commands, moves between positions, and updates its end-position sensor signals.
- Components expose named input and output signals that can be connected to control logic.
- Start with a simple built-in control logic or simulated PLC for the first prototype.
- Design the system so external PLC connections, including OPC UA, can be added later.
- Support component creation, deletion, duplication, and reusable machine assemblies.

## 3D renderer

- Import glTF meshes with materials and textures.
- Provide an editor gizmo for positioning objects in the world.
- Support a PBR material workflow.
- Support image-based lighting (IBL) with HDRIs from [Poly Haven](https://polyhaven.com/hdri).
- Support soft shadow maps.
- Support good SSAO.
- Use an HDR pipeline with tone mapping.

## Behavior and scripting

- Use Lua (or another chosen language) for scripting.
- Allow scripts to control entities and components and run in an update loop.
- Support entity destruction and creation.
- Support spawning new entities and prefabs.
