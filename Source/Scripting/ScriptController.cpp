#include "FactoryCore/ScriptController.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace FactoryCore
{
	struct ScriptController::Context
	{
		lua_State* State = nullptr;
		Machine* CurrentMachine = nullptr;
		std::map<std::string, MachineDefinition, std::less<>> Prefabs;
		int InstructionBudget = 100000;
		int RemainingInstructions = 0;
		bool Failed = false;

		~Context() { if (State) lua_close(State); }

		static Context& Get(lua_State* state)
		{
			return **static_cast<Context**>(lua_getextraspace(state));
		}

		static Machine& GetMachine(lua_State* state)
		{
			auto* machine = Get(state).CurrentMachine;
			if (!machine)
				throw std::logic_error("Engine mutations are only available during OnUpdate");
			return *machine;
		}

		static std::string String(lua_State* state, int index)
		{
			if (lua_type(state, index) != LUA_TSTRING)
				throw std::invalid_argument("Expected a string");
			std::size_t size = 0;
			const auto* text = lua_tolstring(state, index, &size);
			return std::string(text, size);
		}

		static ComponentId Id(lua_State* state, int index)
		{
			if (lua_isinteger(state, index))
			{
				const auto value = lua_tointeger(state, index);
				if (value <= 0) throw std::invalid_argument("Expected a positive component ID");
				return static_cast<ComponentId>(value);
			}
			const auto text = String(state, index);
			ComponentId id = 0;
			const auto result = std::from_chars(text.data(), text.data() + text.size(), id);
			if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size() || id == 0)
				throw std::invalid_argument("Expected a positive component ID");
			return id;
		}

		static double Number(lua_State* state, int index)
		{
			if (lua_type(state, index) != LUA_TNUMBER)
				throw std::invalid_argument("Expected a number");
			return lua_tonumber(state, index);
		}

		static void PushId(lua_State* state, ComponentId id)
		{
			const auto text = std::to_string(id);
			lua_pushlstring(state, text.data(), text.size());
		}

		template<int (*Function)(lua_State*)>
		static int Guard(lua_State* state)
		{
			try { return Function(state); }
			catch (const std::exception& error) { lua_pushstring(state, error.what()); }
			return lua_error(state);
		}

		static int Create(lua_State* state)
		{
			ComponentDefinition definition;
			definition.Kind = ParseEquipmentKind(String(state, 1));
			definition.Name = String(state, 2);
			PushId(state, GetMachine(state).Create(std::move(definition)));
			return 1;
		}

		static int Remove(lua_State* state)
		{
			GetMachine(state).Remove(Id(state, 1));
			return 0;
		}

		static int Duplicate(lua_State* state)
		{
			PushId(state, GetMachine(state).Duplicate(Id(state, 1), String(state, 2)));
			return 1;
		}

		static int SetInput(lua_State* state)
		{
			SignalValue value;
			if (lua_type(state, 3) == LUA_TBOOLEAN) value = lua_toboolean(state, 3) != 0;
			else value = Number(state, 3);
			GetMachine(state).SetInput({ Id(state, 1), String(state, 2) }, value);
			return 0;
		}

		static int GetSignal(lua_State* state)
		{
			const auto value = GetMachine(state).GetOutput({ Id(state, 1), String(state, 2) });
			if (const auto* boolean = std::get_if<bool>(&value)) lua_pushboolean(state, *boolean);
			else lua_pushnumber(state, std::get<double>(value));
			return 1;
		}

		static int Place(lua_State* state)
		{
			auto& machine = GetMachine(state);
			const auto id = Id(state, 1);
			auto definition = machine.GetDefinition(id);
			definition.Placement.Position = { Number(state, 2), Number(state, 3), Number(state, 4) };
			machine.Place(id, definition.Placement);
			return 0;
		}

		static int SetProperty(lua_State* state)
		{
			auto& machine = GetMachine(state);
			const auto id = Id(state, 1);
			auto definition = machine.GetDefinition(id);
			const auto name = String(state, 2);
			const double value = Number(state, 3);
			auto& properties = definition.Properties;
			if (name == "Stroke") properties.Stroke = value;
			else if (name == "Speed") properties.Speed = value;
			else if (name == "Acceleration") properties.Acceleration = value;
			else if (name == "Delay") properties.Delay = value;
			else if (name == "Threshold") properties.Threshold = value;
			else if (name == "Hysteresis") properties.Hysteresis = value;
			else if (name == "InitialPosition") properties.InitialPosition = value;
			else if (name == "Mass") properties.Mass = value;
			else throw std::invalid_argument("Unknown property");
			machine.Configure(id, std::move(definition));
			return 0;
		}

		static int SpawnPrefab(lua_State* state)
		{
			const auto name = String(state, 1);
			const auto iterator = Get(state).Prefabs.find(name);
			if (iterator == Get(state).Prefabs.end())
				throw std::invalid_argument("Unknown prefab name");
			const auto ids = GetMachine(state).SpawnAssembly(iterator->second,
				{ Number(state, 2), Number(state, 3), Number(state, 4) });
			lua_createtable(state, static_cast<int>(ids.size()), 0);
			lua_Integer index = 1;
			for (const auto id : ids)
			{
				PushId(state, id);
				lua_rawseti(state, -2, index++);
			}
			return 1;
		}

		static void InstructionHook(lua_State* state, lua_Debug*)
		{
			auto& context = Get(state);
			context.RemainingInstructions -= 1000;
			if (context.RemainingInstructions <= 0)
			{
				lua_pushliteral(state, "Script instruction budget exceeded");
				lua_error(state);
			}
		}

		void BeginCall()
		{
			RemainingInstructions = InstructionBudget;
			lua_sethook(State, InstructionHook, LUA_MASKCOUNT, 1000);
		}

		std::string TakeError()
		{
			const auto* message = lua_tostring(State, -1);
			std::string error = message ? message : "Lua raised a non-string error";
			lua_pop(State, 1);
			return error;
		}
	};

	ScriptController::ScriptController(std::string_view source, int instructionBudget)
		: m_Context(std::make_unique<Context>())
	{
		if (source.size() > 1024 * 1024 || instructionBudget < 1000 || instructionBudget > 10000000)
			throw std::invalid_argument("Invalid script size or instruction budget");
		auto& context = *m_Context;
		context.InstructionBudget = instructionBudget;
		context.State = luaL_newstate();
		if (!context.State) throw std::bad_alloc();
		auto* state = context.State;
		*static_cast<Context**>(lua_getextraspace(state)) = &context;
		// Trusted local scripts: no operating-system, filesystem or package libraries.
		luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
		lua_pop(state, 1);
		luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
		lua_pop(state, 1);
		luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
		lua_pop(state, 1);
		luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
		lua_pop(state, 1);
		for (const char* name : { "dofile", "loadfile" })
		{
			lua_pushnil(state);
			lua_setglobal(state, name);
		}
		const luaL_Reg bindings[] = {
			{ "Create", Context::Guard<Context::Create> }, { "Remove", Context::Guard<Context::Remove> },
			{ "Duplicate", Context::Guard<Context::Duplicate> }, { "SetInput", Context::Guard<Context::SetInput> },
			{ "GetSignal", Context::Guard<Context::GetSignal> }, { "Place", Context::Guard<Context::Place> },
			{ "SetProperty", Context::Guard<Context::SetProperty> }, { "SpawnPrefab", Context::Guard<Context::SpawnPrefab> },
			{ nullptr, nullptr }
		};
		luaL_newlib(state, bindings);
		lua_setglobal(state, "FactoryCore");
		context.BeginCall();
		if (luaL_loadbufferx(state, source.data(), source.size(), "FactoryCoreScript", "t") != LUA_OK
			|| lua_pcall(state, 0, 0, 0) != LUA_OK)
			throw std::runtime_error(context.TakeError());
		lua_getglobal(state, "OnUpdate");
		const bool hasUpdate = lua_isfunction(state, -1);
		lua_pop(state, 1);
		if (!hasUpdate) throw std::invalid_argument("Script must define OnUpdate(deltaTime, time)");
	}

	ScriptController::~ScriptController() = default;

	void ScriptController::RegisterPrefab(std::string name, MachineDefinition definition)
	{
		if (name.empty() || name.size() > 256)
			throw std::invalid_argument("Invalid prefab name");
		(void)Machine::FromConfiguration(definition);
		m_Context->Prefabs.insert_or_assign(std::move(name), std::move(definition));
	}

	void ScriptController::Apply(Machine& machine)
	{
		auto& context = *m_Context;
		if (context.Failed) throw std::logic_error("Script failed; construct a new controller before resuming");
		auto replacement = machine;
		context.CurrentMachine = &replacement;
		context.BeginCall();
		lua_getglobal(context.State, "OnUpdate");
		lua_pushnumber(context.State, machine.GetTimeStep());
		lua_pushnumber(context.State, machine.GetTime());
		const int result = lua_pcall(context.State, 2, 0, 0);
		context.CurrentMachine = nullptr;
		if (result != LUA_OK)
		{
			context.Failed = true;
			throw std::runtime_error(context.TakeError());
		}
		machine = std::move(replacement);
	}

	bool ScriptController::HasFailed() const { return m_Context->Failed; }

	std::string ReadScriptFile(const std::string& path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input) throw std::runtime_error("Cannot open Lua script");
		std::string text;
		char character = 0;
		while (input.get(character))
		{
			if (text.size() >= 1024 * 1024) throw std::length_error("Lua script exceeds one MiB");
			text.push_back(character);
		}
		if (!input.eof()) throw std::runtime_error("Cannot read Lua script");
		return text;
	}
}
