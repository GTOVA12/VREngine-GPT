# Exact source archives and compiler binary; no moving branches or unverified downloads.
include(FetchContent)
if(NOT WIN32)
	message(FATAL_ERROR "The FactoryCore editor currently targets Windows. Core/runtime support Linux.")
endif()
FetchContent_Declare(fc_donut
	URL https://codeload.github.com/NVIDIA-RTX/Donut/tar.gz/bd498ea39c35abbf764c10b6302ff3bbfee1ebb3
	URL_HASH SHA256=d6932141a8baa117c1d0a2955fcabb3737c9152e8ff0343b9f96389c284215e5
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_donut)
FetchContent_Declare(fc_shadermake
	URL https://codeload.github.com/NVIDIA-RTX/ShaderMake/tar.gz/5daebdbef45088fc2369d441391ecab0eba25e54
	URL_HASH SHA256=e81f36a390284980721295d943adf6d110afc557541b857bb232a1ed5440d1be
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/ShaderMake"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_shadermake)
FetchContent_Declare(fc_nvrhi
	URL https://codeload.github.com/NVIDIA-RTX/NVRHI/tar.gz/d0c8e30d5f8d58c3b838b06aa1d8d0a912bea076
	URL_HASH SHA256=2f62a5115ff8a11384b0d10c336b97aad73a78175addc11ca94c385eff94a452
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/nvrhi"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_nvrhi)
FetchContent_Declare(fc_cgltf
	URL https://codeload.github.com/jkuhlmann/cgltf/tar.gz/fa3b80fa762790192c9532b63c441627416ff300
	URL_HASH SHA256=528d2f414721337b888cc029124a2fc4bd871ce6f68926e3a6c1fdf978d06939
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/thirdparty/cgltf"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_cgltf)
FetchContent_Declare(fc_glfw
	URL https://codeload.github.com/glfw/glfw/tar.gz/7b6aead9fb88b3623e3b3725ebb42670cbe4c579
	URL_HASH SHA256=169d49096e339caabbc12e1762273c5cb9130a535a282ba46cf5c14ab6e55694
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/thirdparty/glfw"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_glfw)
FetchContent_Declare(fc_imgui
	URL https://codeload.github.com/ocornut/imgui/tar.gz/45acd5e0e82f4c954432533ae9985ff0e1aad6d5
	URL_HASH SHA256=97484925aec2f4d3e913d6644d46b234f8d6d8d98c6aa9c50109e0f0df772090
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/thirdparty/imgui"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_imgui)
FetchContent_Declare(fc_stb
	URL https://codeload.github.com/nothings/stb/tar.gz/2e2bef463a5b53ddf8bb788e25da6b8506314c08
	URL_HASH SHA256=4dc2ffa6c6c8d8a830e92fcdb97f8981701c229655df7d60e6fc0ff4e6b4bf66
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_DIR "${fc_donut_SOURCE_DIR}/thirdparty/stb"
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_stb)
FetchContent_Declare(fc_glm
	URL https://codeload.github.com/g-truc/glm/tar.gz/0af55ccecd98d4e5a8d1fad7de25ba429d60e863
	URL_HASH SHA256=e7f187d83523f505eb38dd25d297ea6c0d4ed856d733e808f18253f5a8fa88a0
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_glm)
FetchContent_Declare(fc_imguizmo
	URL https://codeload.github.com/CedricGuillemet/ImGuizmo/tar.gz/18cef5e031d8c6973d80284c67f60549fafd78c1
	URL_HASH SHA256=6ad626f0687be12c2f3ba6542c0f1bdda9e71e395d0645e4cda37695354406d8
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_imguizmo)
FetchContent_Declare(fc_vulkan_headers
	URL https://codeload.github.com/KhronosGroup/Vulkan-Headers/tar.gz/eae540635060031f68af0fe1fecacc1264484a96
	URL_HASH SHA256=664dc0142524f912615c4d2084c823d6c5f7967af3392890ecccee3edaf79e37
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_vulkan_headers)
FetchContent_Declare(fc_dxc
	URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip
	URL_HASH SHA256=a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR FactoryCorePopulationOnly)
FetchContent_MakeAvailable(fc_dxc)
set(SHADERMAKE_FIND_COMPILERS OFF CACHE BOOL "" FORCE)
set(SHADERMAKE_DXC_PATH "${fc_dxc_SOURCE_DIR}/bin/x64/dxc.exe" CACHE FILEPATH "" FORCE)
set(SHADERMAKE_DXC_VK_PATH "${SHADERMAKE_DXC_PATH}" CACHE FILEPATH "" FORCE)
set(DONUT_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_DX12 OFF CACHE BOOL "" FORCE)
set(DONUT_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(DONUT_WITH_STATIC_SHADERS ON CACHE BOOL "" FORCE)
set(DONUT_WITH_KTX OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${fc_vulkan_headers_SOURCE_DIR}" "${fc_vulkan_headers_BINARY_DIR}" SYSTEM)
add_subdirectory("${fc_donut_SOURCE_DIR}" "${fc_donut_BINARY_DIR}" SYSTEM)
add_subdirectory("${fc_glm_SOURCE_DIR}" "${fc_glm_BINARY_DIR}" SYSTEM)
add_library(FactoryCoreGizmo STATIC "${fc_imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp")
target_include_directories(FactoryCoreGizmo SYSTEM PUBLIC "${fc_imguizmo_SOURCE_DIR}/src")
target_link_libraries(FactoryCoreGizmo PUBLIC imgui)
add_executable(FactoryCoreEditor Source/Editor/Main.cpp Source/Editor/SceneRenderer.cpp Source/Editor/EditorUI.cpp)
include("${fc_donut_SOURCE_DIR}/compileshaders.cmake")
donut_compile_shaders(TARGET FactoryCoreShaders
	CONFIG "${PROJECT_SOURCE_DIR}/Source/Editor/Shaders/Shaders.cfg"
	SOURCES Source/Editor/Shaders/FilmicToneMap.hlsl Source/Editor/Shaders/ACES.hlsli
	OUTPUT_FORMAT HEADER SPIRV_DXC "${CMAKE_CURRENT_BINARY_DIR}/Shaders"
	BYPRODUCTS_SPIRV FilmicToneMap.spirv.h
	SHADERMAKE_OPTIONS --WX)
add_dependencies(FactoryCoreEditor FactoryCoreShaders)
target_include_directories(FactoryCoreEditor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
target_link_libraries(FactoryCoreEditor PRIVATE FactoryCoreEditorModel donut_app donut_render donut_engine glm::glm FactoryCoreGizmo)
target_include_directories(FactoryCoreEditor PRIVATE Source/Editor)
target_include_directories(FactoryCoreEditor SYSTEM PRIVATE "${fc_stb_SOURCE_DIR}" "${fc_cgltf_SOURCE_DIR}")
target_compile_definitions(FactoryCoreEditor PRIVATE GLM_ENABLE_EXPERIMENTAL FACTORYCORE_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/Assets")
if(FACTORYCORE_ENABLE_LUA)
	target_link_libraries(FactoryCoreEditor PRIVATE FactoryCoreScripting)
	target_compile_definitions(FactoryCoreEditor PRIVATE FACTORYCORE_WITH_LUA)
endif()
install(TARGETS FactoryCoreEditor RUNTIME DESTINATION bin)
install(DIRECTORY Assets/ DESTINATION share/FactoryCore/Assets)
option(FACTORYCORE_GPU_TESTS "Run Vulkan tests on a supported local GPU" OFF)
if(BUILD_TESTING AND FACTORYCORE_GPU_TESTS)
	target_compile_definitions(FactoryCoreEditor PRIVATE FACTORYCORE_GPU_TESTS)
	target_include_directories(FactoryCoreEditor PRIVATE Tests)
	add_test(NAME EditorUIWorkflow COMMAND FactoryCoreEditor --ui-smoke --capture "${CMAKE_BINARY_DIR}/Testing/FactoryCore-UI-$<CONFIG>.png")
	set_tests_properties(EditorUIWorkflow PROPERTIES TIMEOUT 120)
	add_test(NAME ProductCellEditor COMMAND FactoryCoreEditor --cell-test --capture "${CMAKE_BINARY_DIR}/Testing/ProductCell-$<CONFIG>/Editor-Complete.png")
	set_tests_properties(ProductCellEditor PROPERTIES TIMEOUT 180)
	add_test(NAME VulkanRendering COMMAND FactoryCoreEditor --smoke --capture "${CMAKE_BINARY_DIR}/Testing/FactoryCore-$<CONFIG>.png")
	set_tests_properties(VulkanRendering PROPERTIES TIMEOUT 120)
	add_test(NAME VulkanFailureCleanup COMMAND FactoryCoreEditor --frames 1 --assets "${CMAKE_BINARY_DIR}/MissingAssets")
	set_tests_properties(VulkanFailureCleanup PROPERTIES WILL_FAIL TRUE TIMEOUT 30)
endif()

if(BUILD_TESTING)
	add_test(NAME EditorInvalidArguments COMMAND FactoryCoreEditor --invalid)
	set_tests_properties(EditorInvalidArguments PROPERTIES WILL_FAIL TRUE)
endif()
# Preserve the full upstream notices for every library redistributed in the editor.
foreach(entry IN ITEMS
	"Donut|fc_donut|LICENSE.txt"
	"NVRHI|fc_nvrhi|LICENSE.txt"
	"ShaderMake|fc_shadermake|LICENSE.txt"
	"cgltf|fc_cgltf|LICENSE"
	"GLFW|fc_glfw|LICENSE.md"
	"ImGui|fc_imgui|LICENSE.txt"
	"stb|fc_stb|LICENSE"
	"GLM|fc_glm|copying.txt"
	"ImGuizmo|fc_imguizmo|LICENSE"
	"VulkanHeaders|fc_vulkan_headers|LICENSE.md"
	"VulkanHeadersApache|fc_vulkan_headers|LICENSES/Apache-2.0.txt"
	"VulkanHeadersMIT|fc_vulkan_headers|LICENSES/MIT.txt")
	string(REPLACE "|" ";" parts "${entry}")
	list(GET parts 0 name)
	list(GET parts 1 dependency)
	list(GET parts 2 license)
	install(FILES "${${dependency}_SOURCE_DIR}/${license}" DESTINATION share/FactoryCore/Licenses RENAME "${name}.txt")
endforeach()
install(FILES "${fc_donut_SOURCE_DIR}/thirdparty/jsoncpp-amalgam/LICENSE" DESTINATION share/FactoryCore/Licenses RENAME JsonCpp.txt)
file(READ "${fc_donut_SOURCE_DIR}/thirdparty/tinyexr/tinyexr.h" tinyexrSource)
string(FIND "${tinyexrSource}" "// End of OpenEXR license" headerEnd)
string(SUBSTRING "${tinyexrSource}" 0 ${headerEnd} tinyexrNotice)
string(FIND "${tinyexrSource}" "This is free and unencumbered software" minizStart)
string(FIND "${tinyexrSource}" "// ---------------------- end of miniz" minizEnd)
math(EXPR minizLength "${minizEnd} - ${minizStart}")
string(SUBSTRING "${tinyexrSource}" ${minizStart} ${minizLength} minizNotice)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/TinyEXR-Notice.txt"
	"${tinyexrNotice}\nOpenEXR PIZ code: Copyright (c) 2004, Industrial Light & Magic, under the same BSD license above.\n\nEmbedded miniz:\n${minizNotice}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/TinyEXR-Notice.txt" DESTINATION share/FactoryCore/Licenses RENAME TinyEXR.txt)

install(FILES "${PROJECT_SOURCE_DIR}/Assets/Licenses/BakingLab.txt" DESTINATION share/FactoryCore/Licenses)
