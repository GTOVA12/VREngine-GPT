#include "FactoryCore/ProductCell.h"
#include "FactoryCore/Persistence.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
	void Check(bool value, const char* message)
	{
		if (!value)
		{
			throw std::runtime_error(message);
		}
	}

	FactoryCore::Machine Assemble()
	{
		using namespace FactoryCore;
		Machine machine("Product inspection cell");
		for (const auto& part : GetProductCellParts())
		{
			(void)machine.Create(part);
		}
		for (const auto& wire : GetProductCellConnections(machine))
		{
			machine.Connect(wire);
		}
		return machine;
	}

	std::vector<FactoryCore::ProductCellState> Run(FactoryCore::Machine& machine)
	{
		using namespace FactoryCore;
		ProductCellController plc(machine);
		std::vector<ProductCellState> states{plc.GetState()};
		double heldPosition = 0.0;
		bool captured = false;
		for (unsigned tick = 0; tick < 2500 && plc.GetState() != ProductCellState::Complete; ++tick)
		{
			plc.Apply(machine);
			machine.Step();
			Check(plc.GetState() != ProductCellState::Faulted, "Nominal PLC sequence faulted");
			if (states.back() != plc.GetState())
			{
				states.push_back(plc.GetState());
			}
			const auto phase = plc.GetState();
			if (phase >= ProductCellState::Clamping && phase <= ProductCellState::Releasing)
			{
				const double position = machine.GetState(3).Position;
				if (!captured)
				{
					heldPosition = position;
					captured = true;
				}
				Check(std::abs(position - heldPosition) < 1.e-12, "Product moved while clamped/processed");
				Check(std::abs(position - 1.15) < .08, "Product stopped outside the process station");
			}
			if (phase == ProductCellState::Holding)
			{
				Check(plc.GetIO().Clamped && plc.GetIO().Extended, "Processing started without clamp/head confirmation");
			}
			if (phase == ProductCellState::Discharging)
			{
				Check(machine.GetState(3).Processed && plc.GetIO().Retracted && !plc.GetIO().Clamped, "Unsafe discharge");
			}
		}
		Check(plc.GetState() == ProductCellState::Complete && plc.GetCompletedProducts() == 1, "No completed product");
		Check(machine.GetState(3).Processed && machine.GetState(3).Position > 2.45, "Product not processed/discharged");
		Check(machine.GetState(1).Velocity == 0.0 && machine.GetState(2).Velocity == 0.0, "Drive still running after completion");
		Check(plc.GetScans() == machine.GetTick(), "PLC did not scan once per fixed tick");
		return states;
	}

	void Interrupted(FactoryCore::ProductCellState target, bool emergency)
	{
		using namespace FactoryCore;
		auto machine = Assemble();
		ProductCellController plc(machine);
		for (int tick = 0; tick < 2000 && plc.GetState() != target; ++tick)
		{
			plc.Apply(machine);
			machine.Step();
		}
		Check(plc.GetState() == target, "Interruption phase unreachable");
		if (emergency)
		{
			machine.SetEmergencyStop(true);
		}
		else
		{
			machine.SetInput({7, "Fault"}, true);
		}
		if (!emergency)
		{
			machine.Step();
		}
		plc.Apply(machine);
		machine.Step();
		Check(plc.GetState() == ProductCellState::Faulted && !plc.GetFaultReason().empty(), "PLC did not latch interruption");
		Check(!plc.GetIO().Drive && !plc.GetIO().Extend && !plc.GetIO().Retract, "Unsafe outputs after interruption");
		if (emergency)
		{
			for (auto id : machine.GetComponentIds())
			{
				Check(machine.GetState(id).Velocity == 0.0, "Emergency stop left motion");
			}
			machine.SetEmergencyStop(false);
		}
		for (int tick = 0; tick < 50; ++tick)
		{
			plc.Apply(machine);
			machine.Step();
		}
		Check(plc.GetState() == ProductCellState::Faulted && plc.GetCompletedProducts() == 0, "PLC restarted automatically");
		machine.Reset();
		Check(!machine.GetState(3).Processed && machine.GetTick() == 0, "Reset retained processed/live state");
		(void)Run(machine);
	}
}

int main()
{
	try
	{
		using namespace FactoryCore;
		auto first = Assemble(), second = Assemble();
		const auto states = Run(first);
		Check(states.size() == 11, "PLC missed a required sequence state");
		Check(Run(second) == states && first.GetTick() == second.GetTick() && first.GetState(3).Position == second.GetState(3).Position,
			"PLC replay was nondeterministic");
		for (const auto phase : {ProductCellState::Locating, ProductCellState::Holding, ProductCellState::Discharging})
		{
			Interrupted(phase, true);
		}
		Interrupted(ProductCellState::Holding, false);
		auto invalid = Assemble();
		invalid.Disconnect({3, "Velocity"});
		bool rejected = false;
		try
		{
			ProductCellController controller(invalid);
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		Check(rejected, "PLC accepted missing transport wiring");
		auto timeout = Assemble();
		auto sensor = timeout.GetDefinition(5);
		sensor.Properties.Threshold = 100.0;
		timeout.Configure(5, sensor);
		sensor = timeout.GetDefinition(6);
		sensor.Properties.Threshold = 100.0;
		timeout.Configure(6, sensor);
		ProductCellController timed(timeout);
		for (int tick = 0; tick < 2500 && timed.GetState() != ProductCellState::Faulted; ++tick)
		{
			timed.Apply(timeout);
			timeout.Step();
		}
		Check(timed.GetState() == ProductCellState::Faulted && timed.GetFaultReason() == "PLC sequence timeout",
			"Missing sensor did not time out");
		auto unordered = Assemble();
		auto station = unordered.GetDefinition(5);
		station.Properties.Threshold = -1.0;
		unordered.Configure(5, station);
		ProductCellController invalidOrder(unordered);
		for (int tick = 0; tick < 10 && invalidOrder.GetState() != ProductCellState::Faulted; ++tick)
		{
			invalidOrder.Apply(unordered);
			unordered.Step();
		}
		Check(invalidOrder.GetState() == ProductCellState::Faulted && invalidOrder.GetFaultReason() == "Invalid sensor sequence",
			"Out-of-order feedback did not fault");
		auto competing = Assemble();
		competing.Connect({{4, "Detected"}, {2, "Enable"}});
		rejected = false;
		try
		{
			ProductCellController controller(competing);
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		Check(rejected, "PLC accepted an externally driven output input");
		std::stringstream file;
		SaveConfiguration(first, file);
		auto reloaded = LoadConfiguration(file);
		Check(!reloaded.GetState(3).Processed, "Save persisted a live processed flag");

		auto latch = Assemble();
		latch.SetInput({3, "Processed"}, true);
		latch.Step();
		latch.SetInput({3, "Processed"}, false);
		latch.Step();
		Check(latch.GetState(3).Processed && std::get<bool>(latch.GetOutput({3, "Processed"})), "Inspection flag did not latch");
		latch.Reset();
		Check(!latch.GetState(3).Processed, "Inspection flag survived Reset");
		auto transactional = Assemble();
		ProductCellController plc(transactional);
		transactional.Connect({{4, "Detected"}, {3, "Processed"}});
		rejected = false;
		try
		{
			plc.Apply(transactional);
		}
		catch (const std::invalid_argument&)
		{
			rejected = true;
		}
		Check(rejected && plc.GetScans() == 0 && transactional.GetTick() == 0, "Rejected output scan mutated state");
		Check(!std::get<bool>(transactional.GetInput({2, "Enable"})), "Rejected scan committed a partial drive command");
		std::cout << "PASS product PLC: all sequence states, deterministic replay, held product, safe discharge, three emergency-stop "
					 "phases, equipment fault, timeout, wiring rejection and rollback\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}