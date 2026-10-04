#include "FactoryCore/Controller.h"
#include "FactoryCore/Persistence.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	using namespace FactoryCore;

	void Check(bool condition, const char* expression, int line)
	{
		if (!condition)
			throw std::runtime_error(std::string("Line ") + std::to_string(line) + ": " + expression);
	}
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

	void Near(double actual, double expected, double tolerance = 1.0e-10)
	{
		if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
			throw std::runtime_error("Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
	}

	template<typename Function>
	void Throws(Function&& function)
	{
		bool threw = false;
		try { function(); }
		catch (const std::exception&) { threw = true; }
		CHECK(threw);
	}

	ComponentId Add(Machine& machine, EquipmentKind kind = EquipmentKind::Cylinder)
	{
		ComponentDefinition definition;
		definition.Name = std::string(ToString(kind));
		definition.Kind = kind;
		return machine.Create(definition);
	}

	void CylinderTravel()
	{
		Machine machine("Travel", 0.1);
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		for (int tick = 0; tick < 10; ++tick) machine.Step();
		Near(machine.GetState(id).Position, 1.0);
		CHECK(std::get<bool>(machine.GetOutput({ id, "Extended" })));
		machine.Step();
		CHECK(!machine.GetState(id).Active);
		machine.SetInput({ id, "Extend" }, false);
		machine.SetInput({ id, "Retract" }, true);
		for (int tick = 0; tick < 10; ++tick) machine.Step();
		Near(machine.GetState(id).Position, 0.0);
		CHECK(std::get<bool>(machine.GetOutput({ id, "Retracted" })));
		CHECK(machine.GetTick() == 21);
		Near(machine.GetTime(), 2.1);
	}

	void CylinderPartialStep()
	{
		Machine machine("Partial", 0.3);
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		for (int tick = 0; tick < 4; ++tick) machine.Step();
		Near(machine.GetState(id).Position, 1.0);
		Near(machine.GetState(id).Velocity, 1.0 / 3.0);
		machine.SetInput({ id, "Extend" }, false);
		machine.Step();
		Near(machine.GetState(id).Position, 1.0);
		Near(machine.GetState(id).Velocity, 0.0);
	}

	void CylinderFaultLatch()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.SetInput({ id, "Retract" }, true);
		machine.Step();
		CHECK(machine.GetState(id).Fault == FaultCode::ConflictingCommands);
		Near(machine.GetState(id).Position, 0.0);
		Throws([&] { machine.ResetFault(id); });
		machine.SetInput({ id, "Retract" }, false);
		machine.Step();
		CHECK(std::get<bool>(machine.GetOutput({ id, "Fault" })));
		Throws([&] { machine.ResetFault(id); });
		machine.SetInput({ id, "Extend" }, false);
		machine.ResetFault(id);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		CHECK(machine.GetState(id).Position > 0.0);
	}

	void ExternalFault()
	{
		for (const auto kind : { EquipmentKind::Cylinder, EquipmentKind::Sensor, EquipmentKind::Actuator,
			EquipmentKind::Motor, EquipmentKind::Conveyor, EquipmentKind::Workpiece })
		{
			Machine machine;
			const auto id = Add(machine, kind);
			machine.SetInput({ id, "Fault" }, true);
			machine.Step();
			CHECK(machine.GetState(id).Fault == FaultCode::External);
			CHECK(!machine.GetState(id).Active);
			Near(machine.GetState(id).Velocity, 0.0);
			Throws([&] { machine.ResetFault(id); });
			machine.SetInput({ id, "Fault" }, false);
			machine.Step();
			CHECK(std::get<bool>(machine.GetOutput({ id, "Fault" })));
			machine.ResetFault(id);
			CHECK(!std::get<bool>(machine.GetOutput({ id, "Fault" })));
		}
	}

	void EmergencyStop()
	{
		Machine machine;
		const auto cylinder = Add(machine);
		const auto motor = Add(machine, EquipmentKind::Motor);
		const auto piece = Add(machine, EquipmentKind::Workpiece);
		machine.SetInput({ cylinder, "Extend" }, true);
		machine.SetInput({ motor, "Enable" }, true);
		machine.SetInput({ piece, "Velocity" }, 4.0);
		machine.Step();
		const auto position = machine.GetState(piece).Position;
		machine.SetEmergencyStop(true);
		machine.Step();
		for (const auto id : machine.GetComponentIds())
		{
			Near(machine.GetState(id).Velocity, 0.0);
			CHECK(!machine.GetState(id).Active);
		}
		Near(machine.GetState(piece).Position, position);
		machine.SetEmergencyStop(false);
		machine.Step();
		CHECK(machine.GetState(piece).Position > position);
	}

	void SensorHysteresis()
	{
		Machine machine;
		ComponentDefinition definition;
		definition.Name = "Sensor";
		definition.Kind = EquipmentKind::Sensor;
		definition.Properties.Threshold = 0.8;
		definition.Properties.Hysteresis = 0.2;
		const auto id = machine.Create(definition);
		for (const auto& [position, detected] : std::vector<std::pair<double, bool>>{
			{ 0.79, false }, { 0.8, true }, { 0.7, true }, { 0.59, false }, { 0.7, false } })
		{
			machine.SetInput({ id, "Position" }, position);
			machine.Step();
			CHECK(std::get<bool>(machine.GetOutput({ id, "Detected" })) == detected);
		}
	}

	void ActuatorDelay()
	{
		Machine machine("Actuator", 0.1);
		ComponentDefinition definition;
		definition.Name = "Actuator";
		definition.Kind = EquipmentKind::Actuator;
		definition.Properties.Delay = 0.3;
		const auto id = machine.Create(definition);
		machine.SetInput({ id, "Enable" }, true);
		machine.Step();
		machine.Step();
		CHECK(!machine.GetState(id).Active);
		machine.Step();
		CHECK(machine.GetState(id).Active);
		machine.SetInput({ id, "Enable" }, false);
		machine.Step();
		machine.SetInput({ id, "Enable" }, true);
		machine.Step();
		CHECK(machine.GetState(id).Active);
		machine.SetInput({ id, "Enable" }, false);
		for (int tick = 0; tick < 3; ++tick) machine.Step();
		CHECK(!machine.GetState(id).Active);
		Near(machine.GetState(id).Position, 0.0);
	}

	void MotorAcceleration()
	{
		Machine machine("Motor", 0.1);
		const auto id = Add(machine, EquipmentKind::Motor);
		machine.SetInput({ id, "Enable" }, true);
		for (int tick = 0; tick < 10; ++tick) machine.Step();
		Near(machine.GetState(id).Velocity, 1.0);
		Near(machine.GetState(id).Position, 0.55);
		machine.SetInput({ id, "Reverse" }, true);
		for (int tick = 0; tick < 20; ++tick) machine.Step();
		Near(machine.GetState(id).Velocity, -1.0);
		machine.SetInput({ id, "Enable" }, false);
		for (int tick = 0; tick < 11; ++tick) machine.Step();
		Near(machine.GetState(id).Velocity, 0.0);
		CHECK(!machine.GetState(id).Active);
	}

	void ConveyorWorkpiece()
	{
		Machine machine("Transport", 0.1);
		const auto conveyor = Add(machine, EquipmentKind::Conveyor);
		const auto piece = Add(machine, EquipmentKind::Workpiece);
		machine.Connect({ { conveyor, "Velocity" }, { piece, "Velocity" } });
		machine.SetInput({ conveyor, "Enable" }, true);
		machine.Step();
		Near(machine.GetState(piece).Position, 0.0);
		machine.Step();
		Near(machine.GetState(piece).Position, 0.01);
		machine.SetInput({ piece, "Enable" }, false);
		machine.Step();
		Near(machine.GetState(piece).Position, 0.01);
		Near(machine.GetState(piece).Velocity, 0.0);
	}

	void ConfigurationValidation()
	{
		Throws([] { Machine machine("", 0.01); });
		for (const double timeStep : { 0.0, -0.01, 1.1, std::numeric_limits<double>::quiet_NaN(),
			std::numeric_limits<double>::infinity() })
			Throws([&] { Machine machine("Invalid", timeStep); });
		Machine machine;
		ComponentDefinition definition;
		definition.Name = "Cylinder";
		for (const double speed : { 0.0, -1.0, 1.0e7, std::numeric_limits<double>::quiet_NaN() })
		{
			definition.Properties.Speed = speed;
			Throws([&] { machine.Create(definition); });
		}
		definition.Properties.Speed = 1.0;
		definition.Properties.InitialPosition = 2.0;
		Throws([&] { machine.Create(definition); });
		definition.Properties.InitialPosition = 0.0;
		definition.Placement.Scale.X = 0.0;
		Throws([&] { machine.Create(definition); });
		definition.Placement.Scale.X = 1.0;
		definition.Kind = static_cast<EquipmentKind>(100);
		Throws([&] { machine.Create(definition); });
		CHECK(machine.GetComponentIds().empty());
	}

	void SignalValidation()
	{
		Machine machine;
		const auto id = Add(machine);
		Throws([&] { machine.SetInput({ id, "Extend" }, 1.0); });
		Throws([&] { machine.SetInput({ id, "Missing" }, true); });
		Throws([&] { machine.GetOutput({ id, "Extend" }); });
		Throws([&] { machine.GetInput({ 0, "Extend" }); });
		const auto sensor = Add(machine, EquipmentKind::Sensor);
		Throws([&] { machine.SetInput({ sensor, "Position" }, true); });
		Throws([&] { machine.SetInput({ sensor, "Position" }, std::numeric_limits<double>::infinity()); });
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
	}

	void ConnectionValidation()
	{
		Machine machine;
		const auto cylinder = Add(machine);
		const auto sensor = Add(machine, EquipmentKind::Sensor);
		Throws([&] { machine.Connect({ { cylinder, "Extended" }, { sensor, "Position" } }); });
		Throws([&] { machine.Connect({ { cylinder, "Unknown" }, { sensor, "Position" } }); });
		Throws([&] { machine.Connect({ { cylinder, "Position" }, { 900, "Position" } }); });
		machine.Connect({ { cylinder, "Position" }, { sensor, "Position" } });
		Throws([&] { machine.Connect({ { cylinder, "Position" }, { sensor, "Position" } }); });
		Throws([&] { machine.SetInput({ sensor, "Position" }, 0.0); });
		CHECK(machine.GetConnections().size() == 1);
	}

	void SignalLatency()
	{
		Machine machine("Latency", 0.5);
		const auto cylinder = Add(machine);
		const auto sensor = Add(machine, EquipmentKind::Sensor);
		machine.Connect({ { cylinder, "Position" }, { sensor, "Position" } });
		machine.SetInput({ cylinder, "Extend" }, true);
		machine.Step();
		Near(machine.GetState(cylinder).Position, 0.5);
		Near(machine.GetState(sensor).Position, 0.0);
		machine.Step();
		Near(machine.GetState(sensor).Position, 0.5);
		CHECK(std::get<bool>(machine.GetOutput({ sensor, "Detected" })));
	}

	void OrderIndependence()
	{
		Machine first("First", 0.1), second("Second", 0.1);
		const auto firstCylinder = Add(first);
		const auto firstSensor = Add(first, EquipmentKind::Sensor);
		const auto secondSensor = Add(second, EquipmentKind::Sensor);
		const auto secondCylinder = Add(second);
		first.Connect({ { firstCylinder, "Position" }, { firstSensor, "Position" } });
		second.Connect({ { secondCylinder, "Position" }, { secondSensor, "Position" } });
		first.SetInput({ firstCylinder, "Extend" }, true);
		second.SetInput({ secondCylinder, "Extend" }, true);
		for (int tick = 0; tick < 20; ++tick)
		{
			first.Step();
			second.Step();
			CHECK(first.GetOutput({ firstSensor, "Detected" }) == second.GetOutput({ secondSensor, "Detected" }));
			Near(first.GetState(firstSensor).Position, second.GetState(secondSensor).Position);
		}
	}

	void FeedbackSampling()
	{
		Machine machine("Feedback", 0.1);
		const auto first = Add(machine, EquipmentKind::Actuator);
		const auto second = Add(machine, EquipmentKind::Actuator);
		machine.SetInput({ first, "Enable" }, true);
		machine.Step();
		machine.Connect({ { first, "Active" }, { second, "Enable" } });
		machine.Connect({ { second, "Active" }, { first, "Enable" } });
		for (int tick = 0; tick < 10; ++tick)
		{
			machine.Step();
			CHECK(machine.GetState(first).Active == (tick % 2 != 0));
			CHECK(machine.GetState(second).Active == (tick % 2 == 0));
		}
	}

	void DisconnectClearsInput()
	{
		Machine machine;
		const auto actuator = Add(machine, EquipmentKind::Actuator);
		const auto motor = Add(machine, EquipmentKind::Motor);
		machine.SetInput({ actuator, "Enable" }, true);
		machine.Step();
		machine.Connect({ { actuator, "Active" }, { motor, "Enable" } });
		machine.Step();
		CHECK(std::get<bool>(machine.GetInput({ motor, "Enable" })));
		machine.Disconnect({ motor, "Enable" });
		CHECK(!std::get<bool>(machine.GetInput({ motor, "Enable" })));
		Throws([&] { machine.Disconnect({ motor, "Enable" }); });
		machine.SetInput({ motor, "Enable" }, true);
	}

	void RemoveClearsConnections()
	{
		Machine machine;
		const auto source = Add(machine, EquipmentKind::Actuator);
		const auto target = Add(machine, EquipmentKind::Motor);
		machine.SetInput({ source, "Enable" }, true);
		machine.Step();
		machine.Connect({ { source, "Active" }, { target, "Enable" } });
		machine.Step();
		machine.Remove(source);
		CHECK(machine.GetConnections().empty());
		CHECK(!std::get<bool>(machine.GetInput({ target, "Enable" })));
		Throws([&] { machine.GetState(source); });
		Throws([&] { machine.Remove(source); });
		const auto next = Add(machine);
		CHECK(next > target);
	}

	void DuplicationResetsState()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		const auto duplicate = machine.Duplicate(id, "Copy");
		CHECK(duplicate != id);
		Near(machine.GetState(duplicate).Position, 0.0);
		CHECK(!std::get<bool>(machine.GetInput({ duplicate, "Extend" })));
		CHECK(machine.GetDefinition(duplicate).Name == "Copy");
	}

	void ConfigureResetsState()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		auto definition = machine.GetDefinition(id);
		definition.Properties.Stroke = 2.0;
		definition.Properties.InitialPosition = 1.0;
		machine.Configure(id, definition);
		Near(machine.GetState(id).Position, 1.0);
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
		definition.Kind = EquipmentKind::Motor;
		Throws([&] { machine.Configure(id, definition); });
		CHECK(machine.GetDefinition(id).Kind == EquipmentKind::Cylinder);
	}

	void ResetRestoresConfiguration()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		machine.SetEmergencyStop(true);
		const auto removed = Add(machine);
		machine.Remove(removed);
		machine.Reset();
		CHECK(Add(machine) > removed);
		CHECK(machine.GetTick() == 0);
		CHECK(!machine.IsEmergencyStopped());
		Near(machine.GetState(id).Position, 0.0);
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
	}

	void AssemblyRemapping()
	{
		Machine source("Assembly");
		const auto cylinder = Add(source);
		const auto sensor = Add(source, EquipmentKind::Sensor);
		source.Connect({ { cylinder, "Position" }, { sensor, "Position" } });
		Machine target("Line");
		Add(target);
		const auto first = target.SpawnAssembly(source.GetConfiguration(), { 2.0, 3.0, 4.0 });
		const auto second = target.SpawnAssembly(source.GetConfiguration(), { 8.0, 0.0, 0.0 });
		CHECK(first.size() == 2 && second.size() == 2);
		CHECK(first[0] != second[0]);
		Near(target.GetDefinition(first[0]).Placement.Position.X, 2.0);
		CHECK(target.GetConnections()[0].Source.Component == first[0]);
		CHECK(target.GetConnections()[0].Destination.Component == first[1]);
		CHECK(target.GetConnections()[1].Source.Component == second[0]);
		target.SetInput({ first[0], "Extend" }, true);
		target.Step();
		target.Step();
		CHECK(target.GetState(first[1]).Position > 0.0);
		Near(target.GetState(second[1]).Position, 0.0);
	}

	void TransactionalConfiguration()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		auto invalid = machine.GetConfiguration();
		invalid.Components[0].Properties.Speed = -1.0;
		Throws([&] { machine.ReplaceConfiguration(invalid); });
		Throws([&] { machine.SpawnAssembly(invalid); });
		CHECK(machine.GetTick() == 1);
		Near(machine.GetState(id).Position, 0.01);
		CHECK(machine.GetComponentIds().size() == 1);
		auto valid = machine.GetConfiguration();
		Throws([&] { machine.SpawnAssembly(valid, { std::numeric_limits<double>::infinity(), 0.0, 0.0 }); });
		CHECK(machine.GetComponentIds().size() == 1);
	}

	void IdentityValidation()
	{
		Machine machine;
		const auto id = Add(machine);
		auto definition = machine.GetConfiguration();
		definition.Components.push_back(definition.Components[0]);
		Throws([&] { Machine::FromConfiguration(definition); });
		definition.Components.pop_back();
		definition.Components[0].Id = 0;
		Throws([&] { Machine::FromConfiguration(definition); });
		definition.Components[0].Id = std::numeric_limits<ComponentId>::max();
		Throws([&] { Machine::FromConfiguration(definition); });
		definition.Components[0].Id = std::numeric_limits<ComponentId>::max() - 1;
		auto fullIds = Machine::FromConfiguration(definition);
		Throws([&] { Add(fullIds); });
		CHECK(fullIds.GetComponentIds().size() == 1);
		CHECK(id == 1);
	}

	void PersistenceRoundTrip()
	{
		Machine machine("Cell \"quoted\" \\ UTF-8", 0.012345678901234);
		for (const auto kind : { EquipmentKind::Cylinder, EquipmentKind::Sensor, EquipmentKind::Actuator,
			EquipmentKind::Motor, EquipmentKind::Conveyor, EquipmentKind::Workpiece })
		{
			ComponentDefinition definition;
			definition.Name = std::string(ToString(kind));
			definition.Kind = kind;
			definition.VisualModel = "Assets/mesh with spaces.gltf";
			definition.Placement.Position = { 1.2, -3.4, 5.6 };
			definition.Placement.Rotation = { 0.1, 0.2, 0.3 };
			definition.Placement.Scale = { 2.0, 3.0, 4.0 };
			definition.Properties.Speed = 0.123456789012345;
			machine.Create(definition);
		}
		machine.Connect({ { 1, "Position" }, { 2, "Position" } });
		std::stringstream first;
		SaveConfiguration(machine, first);
		const auto loaded = LoadConfiguration(first);
		std::stringstream second;
		SaveConfiguration(loaded, second);
		CHECK(first.str() == second.str());
		CHECK(loaded.GetComponentIds() == machine.GetComponentIds());
		CHECK(loaded.GetConnections() == machine.GetConnections());
		CHECK(loaded.GetTimeStep() == machine.GetTimeStep());
	}

	void PersistenceResetsLiveState()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		machine.SetEmergencyStop(true);
		std::stringstream text;
		SaveConfiguration(machine, text);
		auto loaded = LoadConfiguration(text);
		CHECK(loaded.GetTick() == 0);
		CHECK(!loaded.IsEmergencyStopped());
		CHECK(!std::get<bool>(loaded.GetInput({ id, "Extend" })));
		Near(loaded.GetState(id).Position, 0.0);
	}

	void MalformedConfigurations()
	{
		for (const auto& text : std::vector<std::string>{
			"", "OTHER 1", "FACTORYCORE 2", "FACTORYCORE 1 machine \"x\" 0 components 0 connections 0 end",
			"FACTORYCORE 1 machine \"x\" 0.01 components -1",
			"FACTORYCORE 1 machine \"x\" 0.01 components 10001",
			"FACTORYCORE 1 machine \"x\" 0.01 components 0 connections 100001",
			"FACTORYCORE 1 machine \"x\" 0.01 components 0 connections 0 end junk",
			"FACTORYCORE 1 machine \"x\" 0.01 components 1 component -2",
			"FACTORYCORE 1 machine \"x\" 0.01 components 1 component 18446744073709551616",
			"FACTORYCORE 1 machine \"x\" 0.01 components 1 component 1 \"x\" Unknown \"\"",
			"FACTORYCORE 1 machine \"x\" 0.01 components 0 connections 1 connection 1 \"Position\" 2 \"Position\" end"
		})
		{
			std::istringstream input(text);
			Throws([&] { LoadConfiguration(input); });
		}
		Machine machine;
		Add(machine);
		std::ostringstream output;
		SaveConfiguration(machine, output);
		const auto valid = output.str();
		for (std::size_t length = 0; length < valid.size() - 4; ++length)
		{
			std::istringstream truncated(valid.substr(0, length));
			Throws([&] { LoadConfiguration(truncated); });
		}
	}

	void PersistenceSizeLimit()
	{
		std::istringstream huge(std::string(8 * 1024 * 1024 + 1, 'x'));
		Throws([&] { LoadConfiguration(huge); });
	}

	void StreamFailures()
	{
		Machine machine;
		std::ostringstream output;
		output.setstate(std::ios::badbit);
		Throws([&] { SaveConfiguration(machine, output); });
		std::istringstream input;
		input.setstate(std::ios::badbit);
		Throws([&] { LoadConfiguration(input); });
	}

	void FilePersistence()
	{
		// Each invocation gets an isolated directory inside the build working directory.
		const auto directory = std::filesystem::current_path() / ("FactoryCoreTest-" + std::to_string(std::random_device{}()));
		CHECK(std::filesystem::create_directory(directory));
		const auto path = directory / "Machine.factory";
		try
		{
			Machine first("First");
			Add(first);
			SaveConfigurationFile(first, path);
			CHECK(LoadConfigurationFile(path).GetName() == "First");
			Machine second("Second");
			Add(second, EquipmentKind::Workpiece);
			SaveConfigurationFile(second, path);
			CHECK(LoadConfigurationFile(path).GetName() == "Second");
			Throws([&] { SaveConfigurationFile(second, directory / "Missing" / "Machine.factory"); });
			CHECK(LoadConfigurationFile(path).GetName() == "Second");
			Throws([&] { LoadConfigurationFile(directory / "Absent.factory"); });
			CHECK(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator{}) == 1);
		}
		catch (...)
		{
			std::filesystem::remove(path);
			std::filesystem::remove(directory);
			throw;
		}
		std::filesystem::remove(path);
		std::filesystem::remove(directory);
	}

	void ControllerCycles()
	{
		Machine machine("Cycle", 0.01);
		const auto id = Add(machine);
		CylinderCycleController controller(id, 3);
		controller.Start();
		Throws([&] { controller.Start(); });
		for (int tick = 0; tick < 1000 && controller.GetState() != CycleState::Complete; ++tick)
		{
			controller.Apply(machine);
			machine.Step();
		}
		CHECK(controller.GetState() == CycleState::Complete);
		CHECK(controller.GetCompletedCycles() == 3);
		Near(machine.GetState(id).Position, 0.0);
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
		CHECK(!std::get<bool>(machine.GetInput({ id, "Retract" })));
		controller.Start();
		CHECK(controller.GetCompletedCycles() == 0);
	}

	void ControllerFaults()
	{
		Throws([] { CylinderCycleController controller(0); });
		Throws([] { CylinderCycleController controller(1, 0); });
		Machine machine;
		const auto id = Add(machine);
		CylinderCycleController controller(id);
		controller.Start();
		controller.Apply(machine);
		machine.Step();
		machine.SetEmergencyStop(true);
		controller.Apply(machine);
		CHECK(controller.GetState() == CycleState::Faulted);
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
		machine.SetEmergencyStop(false);
		controller.Apply(machine);
		CHECK(controller.GetState() == CycleState::Faulted);
		const auto motor = Add(machine, EquipmentKind::Motor);
		CylinderCycleController invalid(motor);
		Throws([&] { invalid.Apply(machine); });
	}

	void DeterministicReplay()
	{
		Machine first("Replay");
		const auto cylinder = Add(first);
		const auto sensor = Add(first, EquipmentKind::Sensor);
		first.Connect({ { cylinder, "Position" }, { sensor, "Position" } });
		auto second = Machine::FromConfiguration(first.GetConfiguration());
		std::mt19937 random(1234);
		for (int tick = 0; tick < 10000; ++tick)
		{
			const bool extend = random() % 2 == 0;
			for (auto* machine : { &first, &second })
			{
				machine->SetInput({ cylinder, "Extend" }, extend);
				machine->SetInput({ cylinder, "Retract" }, !extend);
				machine->Step();
			}
			CHECK(first.GetState(cylinder).Position == second.GetState(cylinder).Position);
			CHECK(first.GetOutput({ sensor, "Detected" }) == second.GetOutput({ sensor, "Detected" }));
		}
		CHECK(first.GetTick() == 10000);
	}

	void NumericMotionBound()
	{
		Machine machine("Bound", 1.0);
		const auto id = Add(machine, EquipmentKind::Workpiece);
		machine.SetInput({ id, "Velocity" }, 1.0e12);
		machine.Step();
		machine.Step();
		CHECK(machine.GetState(id).Fault == FaultCode::External);
		CHECK(!machine.GetState(id).Active);
		Near(machine.GetState(id).Velocity, 0.0);
		CHECK(std::isfinite(machine.GetState(id).Position));
	}
	void PlacementPreservesState()
	{
		Machine machine;
		const auto id = Add(machine);
		machine.SetInput({ id, "Extend" }, true);
		machine.Step();
		auto placement = machine.GetDefinition(id).Placement;
		placement.Position.X = 10.0;
		machine.Place(id, placement);
		Near(machine.GetState(id).Position, 0.01);
		CHECK(std::get<bool>(machine.GetInput({ id, "Extend" })));
		machine.Step();
		Near(machine.GetState(id).Position, 0.02);
		placement.Scale.X = -1.0;
		Throws([&] { machine.Place(id, placement); });
		Near(machine.GetDefinition(id).Placement.Scale.X, 1.0);
	}

	void ControllerRejectsWiringAtomically()
	{
		Machine machine;
		const auto id = Add(machine);
		const auto actuator = Add(machine, EquipmentKind::Actuator);
		machine.Connect({ { actuator, "Active" }, { id, "Retract" } });
		CylinderCycleController controller(id);
		controller.Start();
		Throws([&] { controller.Apply(machine); });
		CHECK(!std::get<bool>(machine.GetInput({ id, "Extend" })));
		CHECK(controller.GetState() == CycleState::Extending);
	}

}

int main(int argc, char** argv)
{
	const std::vector<std::pair<std::string, std::function<void()>>> tests{
		{ "CylinderTravel", CylinderTravel }, { "CylinderPartialStep", CylinderPartialStep },
		{ "CylinderFaultLatch", CylinderFaultLatch }, { "ExternalFault", ExternalFault },
		{ "EmergencyStop", EmergencyStop }, { "SensorHysteresis", SensorHysteresis },
		{ "ActuatorDelay", ActuatorDelay }, { "MotorAcceleration", MotorAcceleration },
		{ "ConveyorWorkpiece", ConveyorWorkpiece }, { "ConfigurationValidation", ConfigurationValidation },
		{ "SignalValidation", SignalValidation }, { "ConnectionValidation", ConnectionValidation },
		{ "SignalLatency", SignalLatency }, { "OrderIndependence", OrderIndependence },
		{ "FeedbackSampling", FeedbackSampling }, { "DisconnectClearsInput", DisconnectClearsInput },
		{ "RemoveClearsConnections", RemoveClearsConnections }, { "DuplicationResetsState", DuplicationResetsState },
		{ "ConfigureResetsState", ConfigureResetsState }, { "ResetRestoresConfiguration", ResetRestoresConfiguration },
		{ "AssemblyRemapping", AssemblyRemapping }, { "TransactionalConfiguration", TransactionalConfiguration },
		{ "IdentityValidation", IdentityValidation }, { "PersistenceRoundTrip", PersistenceRoundTrip },
		{ "PersistenceResetsLiveState", PersistenceResetsLiveState }, { "MalformedConfigurations", MalformedConfigurations },
		{ "PersistenceSizeLimit", PersistenceSizeLimit }, { "StreamFailures", StreamFailures },
		{ "FilePersistence", FilePersistence }, { "ControllerCycles", ControllerCycles },
		{ "ControllerFaults", ControllerFaults }, { "DeterministicReplay", DeterministicReplay },
		{ "NumericMotionBound", NumericMotionBound },
		{ "PlacementPreservesState", PlacementPreservesState }, { "ControllerRejectsWiringAtomically", ControllerRejectsWiringAtomically }
	};
	int failures = 0;
	int executed = 0;
	for (const auto& [name, test] : tests)
	{
		if (argc == 2 && name != argv[1])
			continue;
		++executed;
		try { test(); std::cout << "PASS " << name << '\n'; }
		catch (const std::exception& error)
		{
			++failures;
			std::cerr << "FAIL " << name << ": " << error.what() << '\n';
		}
	}
	std::cout << executed << " tests, " << failures << " failures\n";
	return failures == 0 && executed > 0 && argc <= 2 ? 0 : 1;
}
