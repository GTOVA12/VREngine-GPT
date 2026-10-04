# Third-party notices

Complete redistribution notices are included under share/FactoryCore/Licenses in installed packages.

| Dependency | Origin | License |
| --- | --- | --- |
| Lua 5.5.1 | https://www.lua.org/ | MIT |
| Donut | https://github.com/NVIDIA-RTX/Donut | MIT |
| NVRHI | https://github.com/NVIDIA-RTX/NVRHI | MIT |
| ShaderMake | https://github.com/NVIDIA-RTX/ShaderMake | MIT |
| GLFW | https://github.com/glfw/glfw | zlib |
| GLM 1.0.1 | https://github.com/g-truc/glm | MIT / modified MIT |
| Dear ImGui | https://github.com/ocornut/imgui | MIT |
| ImGuizmo | https://github.com/CedricGuillemet/ImGuizmo | MIT |
| cgltf | https://github.com/jkuhlmann/cgltf | MIT |
| stb | https://github.com/nothings/stb | MIT / public domain |
| JsonCpp (Donut amalgamation) | https://github.com/open-source-parsers/jsoncpp | MIT |
| TinyEXR / OpenEXR code | Donut bundled header | BSD; embedded miniz public domain |
| Vulkan-Headers | https://github.com/KhronosGroup/Vulkan-Headers | Apache 2.0 / MIT |

The exact graphics source revisions and SHA-256 values are recorded in CMake/Editor.cmake. Donut's bundled JsonCpp and TinyEXR are covered by the Donut source hash. Lua's official source archive is verified in CMake/Lua.cmake.

Microsoft DirectXShaderCompiler v1.9.2602 is a build-time dependency, fetched from its official release with SHA-256 a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e. DXC binaries are not redistributed with FactoryCore; generated Vulkan shaders are embedded in the editor.

The bundled Studio Small 09 HDRI is CC0; see Assets/Attribution.md. FactoryCore's renderer orchestration uses Donut's MIT-licensed passes and follows its documented/sample integration conventions; it does not claim authorship of those upstream implementations.
