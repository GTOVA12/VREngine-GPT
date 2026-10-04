#include "FactoryCore/Persistence.h"

#include <charconv>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
	FactoryCore::ComponentId ParseId(std::string_view text)
	{
		FactoryCore::ComponentId id = 0;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), id);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || id == 0)
			throw std::invalid_argument("Expected a positive component ID");
		return id;
	}

	double ParseNumber(std::string_view text)
	{
		double number = 0.0;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(number))
			throw std::invalid_argument("Expected a finite number");
		return number;
	}

	void Inspect(const FactoryCore::Machine& machine)
	{
		std::cout << machine.GetName() << " timeStep=" << machine.GetTimeStep() << '\n';
		for (const auto id : machine.GetComponentIds())
		{
			const auto& definition = machine.GetDefinition(id);
			std::cout << id << " " << FactoryCore::ToString(definition.Kind) << " " << definition.Name
				<< " at " << definition.Placement.Position.X << "," << definition.Placement.Position.Y
				<< "," << definition.Placement.Position.Z << '\n';
			for (const auto& signal : FactoryCore::Machine::GetInputs(definition.Kind))
				std::cout << "  input " << signal.Name << " " << (signal.Type == FactoryCore::SignalType::Boolean ? "bool" : "number") << '\n';
			for (const auto& signal : FactoryCore::Machine::GetOutputs(definition.Kind))
				std::cout << "  output " << signal.Name << " " << (signal.Type == FactoryCore::SignalType::Boolean ? "bool" : "number") << '\n';
		}
		for (const auto& connection : machine.GetConnections())
			std::cout << connection.Source.Component << "." << connection.Source.Signal << " -> "
				<< connection.Destination.Component << "." << connection.Destination.Signal << '\n';
	}

	void SetProperty(FactoryCore::EquipmentProperties& properties, std::string_view name, double value)
	{
		if (name == "Stroke") properties.Stroke = value;
		else if (name == "Speed") properties.Speed = value;
		else if (name == "Acceleration") properties.Acceleration = value;
		else if (name == "Delay") properties.Delay = value;
		else if (name == "Threshold") properties.Threshold = value;
		else if (name == "Hysteresis") properties.Hysteresis = value;
		else if (name == "InitialPosition") properties.InitialPosition = value;
		else if (name == "Mass") properties.Mass = value;
		else throw std::invalid_argument("Unknown property");
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc < 3)
			throw std::invalid_argument("Usage: FactoryCoreAuthor <inspect|new|add|remove|duplicate|place|property|connect|disconnect|spawn> <file> [arguments]");
		const std::string_view command(argv[1]);
		const std::filesystem::path path(argv[2]);
		if (command == "new" && argc == 4)
		{
			FactoryCore::SaveConfigurationFile(FactoryCore::Machine(argv[3]), path);
			return 0;
		}
		auto machine = FactoryCore::LoadConfigurationFile(path);
		if (command == "inspect" && argc == 3)
		{
			Inspect(machine);
			return 0;
		}
		if (command == "add" && argc == 5)
		{
			FactoryCore::ComponentDefinition definition;
			definition.Kind = FactoryCore::ParseEquipmentKind(argv[3]);
			definition.Name = argv[4];
			std::cout << machine.Create(definition) << '\n';
		}
		else if (command == "remove" && argc == 4)
			machine.Remove(ParseId(argv[3]));
		else if (command == "duplicate" && argc == 5)
			std::cout << machine.Duplicate(ParseId(argv[3]), argv[4]) << '\n';
		else if (command == "place" && argc == 7)
		{
			const auto id = ParseId(argv[3]);
			auto definition = machine.GetDefinition(id);
			definition.Placement.Position = { ParseNumber(argv[4]), ParseNumber(argv[5]), ParseNumber(argv[6]) };
			machine.Place(id, definition.Placement);
		}
		else if (command == "property" && argc == 6)
		{
			const auto id = ParseId(argv[3]);
			auto definition = machine.GetDefinition(id);
			SetProperty(definition.Properties, argv[4], ParseNumber(argv[5]));
			machine.Configure(id, definition);
		}
		else if (command == "connect" && argc == 7)
			machine.Connect({ { ParseId(argv[3]), argv[4] }, { ParseId(argv[5]), argv[6] } });
		else if (command == "disconnect" && argc == 5)
			machine.Disconnect({ ParseId(argv[3]), argv[4] });
		else if (command == "spawn" && argc == 7)
		{
			const auto assembly = FactoryCore::LoadConfigurationFile(argv[3]);
			const auto ids = machine.SpawnAssembly(assembly.GetConfiguration(),
				{ ParseNumber(argv[4]), ParseNumber(argv[5]), ParseNumber(argv[6]) });
			for (const auto id : ids)
				std::cout << id << '\n';
		}
		else
			throw std::invalid_argument("Unknown command or incorrect argument count");
		FactoryCore::SaveConfigurationFile(machine, path);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
