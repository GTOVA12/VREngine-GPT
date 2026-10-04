include(FetchContent)
FetchContent_Declare(lua
	URL https://www.lua.org/ftp/lua-5.5.1.tar.gz
	URL_HASH SHA256=1c4b4068d67061f2a2231ad2b5422e77acea1487ea9890f6320af614f4373dce
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lua)
file(GLOB luaSources CONFIGURE_DEPENDS "${lua_SOURCE_DIR}/src/*.c")
list(FILTER luaSources EXCLUDE REGEX "/(lua|luac)\\.c$")
# Lua officially supports C++ compilation. Its error path then unwinds C++ frames
# rather than longjmp-ing over RAII objects inside engine bindings.
set_source_files_properties(${luaSources} PROPERTIES LANGUAGE CXX)
add_library(FactoryCoreLua STATIC ${luaSources})
target_include_directories(FactoryCoreLua PUBLIC "${lua_SOURCE_DIR}/src")
target_compile_features(FactoryCoreLua PRIVATE cxx_std_20)
if(UNIX)
	target_link_libraries(FactoryCoreLua PRIVATE ${CMAKE_DL_LIBS} m)
endif()
install(FILES "${lua_SOURCE_DIR}/doc/readme.html" DESTINATION share/FactoryCore/Licenses RENAME Lua.html)
add_library(FactoryCoreScripting Source/Scripting/ScriptController.cpp)
target_link_libraries(FactoryCoreScripting PUBLIC FactoryCoreSimulation PRIVATE FactoryCoreLua)
target_compile_features(FactoryCoreScripting PUBLIC cxx_std_20)
target_link_libraries(FactoryCoreRuntime PRIVATE FactoryCoreScripting)
target_compile_definitions(FactoryCoreRuntime PRIVATE FACTORYCORE_WITH_LUA=1)
if(BUILD_TESTING)
	add_executable(FactoryCoreScriptTests Tests/ScriptTests.cpp)
	target_link_libraries(FactoryCoreScriptTests PRIVATE FactoryCoreScripting)
	add_test(NAME LuaScripting COMMAND FactoryCoreScriptTests)
	add_test(NAME LuaRuntime COMMAND FactoryCoreRuntime "${CMAKE_CURRENT_SOURCE_DIR}/Examples/CylinderCell.factory" 500 "${CMAKE_CURRENT_SOURCE_DIR}/Examples/CylinderCycle.lua")
endif()
