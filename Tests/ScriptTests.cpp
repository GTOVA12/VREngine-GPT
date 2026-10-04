#include "FactoryCore/ScriptController.h"

#include <iostream>
#include <stdexcept>

namespace
{
	void Check(bool condition)
	{
		if (!condition) throw std::runtime_error("Lua scripting test assertion failed");
	}

	template<typename Function>
	void Throws(Function&& function)
	{
		bool threw = false;
		try { function(); } catch (const std::exception&) { threw = true; }
		Check(threw);
	}
}

int main()
{
	try
	{
		using namespace FactoryCore;
		Machine machine;
		ComponentDefinition cylinder;
		cylinder.Name = "Cylinder";
		const auto id = machine.Create(cylinder);
		Check(id == 1);
		ScriptController control("function OnUpdate(dt, time) FactoryCore.SetInput(1, 'Extend', true) end");
		control.Apply(machine);
		machine.Step();
		Check(machine.GetState(id).Position > 0.0);

		ScriptController lifetime(R"(
			local done = false
			function OnUpdate(dt, time)
				if not done then
					local piece = FactoryCore.Create("Workpiece", "Spawned")
					FactoryCore.Place(piece, 2, 3, 4)
					FactoryCore.SetProperty(piece, "Mass", 2)
					local copy = FactoryCore.Duplicate(piece, "Copy")
					FactoryCore.Remove(piece)
					FactoryCore.SetInput(copy, "Velocity", 1)
					done = true
				end
			end
		)");
		lifetime.Apply(machine);
		Check(machine.GetComponentIds().size() == 2);
		const auto spawned = machine.GetComponentIds().back();
		Check(machine.GetDefinition(spawned).Name == "Copy");
		Check(machine.GetDefinition(spawned).Placement.Position.X == 2);
		machine.Step();
		Check(machine.GetState(spawned).Position > 0.0);

		ScriptController transactional(R"(
			function OnUpdate(dt, time)
				FactoryCore.Create("Motor", "Must roll back")
				FactoryCore.SetInput(1, "Missing", true)
			end
		)");
		const auto before = machine.GetComponentIds();
		Throws([&] { transactional.Apply(machine); });
		Check(transactional.HasFailed());
		Check(machine.GetComponentIds() == before);
		Throws([&] { transactional.Apply(machine); });

		ScriptController budget("function OnUpdate(dt, time) while true do end end", 1000);
		Throws([&] { budget.Apply(machine); });
		Check(machine.GetComponentIds() == before);
		Check(budget.HasFailed());
		Throws([] { ScriptController script("while true do end", 1000); });
		Throws([] { ScriptController script("function broken("); });
		Throws([] { ScriptController script("value = 1"); });
		Throws([] { ScriptController script("FactoryCore.Create('Motor', 'top level') function OnUpdate() end"); });
		ScriptController badType("function OnUpdate() FactoryCore.SetInput(1, 'Extend', 1) end");
		Throws([&] { badType.Apply(machine); });

		ScriptController prefab("function OnUpdate() local ids = FactoryCore.SpawnPrefab('Cell', 5, 0, 0) assert(#ids == 1) end");
		Machine assembly("Assembly");
		cylinder.Id = 0;
		assembly.Create(cylinder);
		prefab.RegisterPrefab("Cell", assembly.GetConfiguration());
		prefab.Apply(machine);
		Check(machine.GetComponentIds().size() == before.size() + 1);
		Check(machine.GetDefinition(machine.GetComponentIds().back()).Placement.Position.X == 5);
		std::cout << "PASS Lua control, creation, placement, properties, duplication, deletion, prefab spawning, rollback, errors and instruction limits\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
