#include "FactoryCore/Controller.h"
#include "FactoryCore/Persistence.h"

#ifdef FACTORYCORE_WITH_LUA
#include "FactoryCore/ScriptController.h"
#endif

#include <charconv>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
	std::uint64_t ParseTicks(std::string_view text)
	{
		std::uint64_t value = 0;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 || value > 10000000)
			throw std::invalid_argument("Ticks must be an integer from 1 to 10000000");
		return value;
	}

	int RunDemo()
	{
		FactoryCore::Machine machine("Cylinder cell");
		FactoryCore::ComponentDefinition cylinder;
		cylinder.Name = "Cylinder";
		cylinder.Properties.Stroke = 0.2;
		cylinder.Properties.Speed = 0.4;
		const auto id = machine.Create(cylinder);
		FactoryCore::ComponentDefinition sensor;
		sensor.Name = "End sensor";
		sensor.Kind = FactoryCore::EquipmentKind::Sensor;
		sensor.Properties.Threshold = 0.2;
		const auto sensorId = machine.Create(sensor);
		machine.Connect({ { id, "Position" }, { sensorId, "Position" } });
		FactoryCore::CylinderCycleController controller(id, 3);
		controller.Start();
		auto previousState = FactoryCore::CycleState::Idle;
		for (std::uint64_t tick = 0; tick < 1000; ++tick)
		{
			controller.Apply(machine);
			machine.Step();
			if (controller.GetState() != previousState)
			{
				previousState = controller.GetState();
				std::cout << "tick=" << machine.GetTick() << " cycleState=" << static_cast<int>(previousState)
					<< " position=" << machine.GetState(id).Position << '\n';
			}
			if (controller.GetState() == FactoryCore::CycleState::Complete)
			{
				std::cout << "Completed " << controller.GetCompletedCycles() << " cycles in " << machine.GetTime() << " s\n";
				return machine.GetState(id).Fault == FactoryCore::FaultCode::None ? 0 : 1;
			}
			if (controller.GetState() == FactoryCore::CycleState::Faulted)
				return 1;
		}
		std::cerr << "Demo did not complete within its tick budget\n";
		return 1;
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc == 2 && std::string_view(argv[1]) == "--demo")
			return RunDemo();
		if (argc != 3 && argc != 4)
			throw std::invalid_argument("Usage: FactoryCoreRuntime --demo | <machine.factory> <ticks> [script.lua]");
		auto machine = FactoryCore::LoadConfigurationFile(argv[1]);
		const auto ticks = ParseTicks(argv[2]);
		std::unique_ptr<FactoryCore::IController> controller;
		if (argc == 4)
		{
#ifdef FACTORYCORE_WITH_LUA
			controller = std::make_unique<FactoryCore::ScriptController>(FactoryCore::ReadScriptFile(argv[3]));
#else
			throw std::invalid_argument("This build has Lua disabled");
#endif
		}
		for (std::uint64_t tick = 0; tick < ticks; ++tick)
		{
			if (controller) controller->Apply(machine);
			machine.Step();
		}
		std::cout << machine.GetName() << ": " << machine.GetTick() << " ticks, " << machine.GetTime() << " s\n";
		bool faulted = false;
		for (const auto id : machine.GetComponentIds())
		{
			const auto& state = machine.GetState(id);
			faulted = faulted || state.Fault != FactoryCore::FaultCode::None;
			std::cout << id << " " << machine.GetDefinition(id).Name << " position=" << state.Position
				<< " active=" << state.Active << " fault=" << static_cast<int>(state.Fault) << '\n';
		}
		return faulted ? 1 : 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
