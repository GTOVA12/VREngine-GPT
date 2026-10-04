#include "FactoryCore/ProductCell.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace FactoryCore
{
	namespace
	{
		constexpr std::array<const char*, 8> s_Names{
			"Conveyor frame", "Drive motor", "Product", "Entry sensor", "Station sensor", "Exit sensor", "Process head", "Product clamp"};
		constexpr std::array<EquipmentKind, 8> s_Kinds{EquipmentKind::Conveyor, EquipmentKind::Motor, EquipmentKind::Workpiece,
			EquipmentKind::Sensor, EquipmentKind::Sensor, EquipmentKind::Sensor, EquipmentKind::Cylinder, EquipmentKind::Actuator};

		std::array<ComponentId, 8> Resolve(const Machine& machine)
		{
			std::array<ComponentId, 8> ids{};
			for (auto id : machine.GetComponentIds())
			{
				for (std::size_t index = 0; index < ids.size(); ++index)
				{
					if (machine.GetDefinition(id).Name == s_Names[index])
					{
						if (ids[index] || machine.GetDefinition(id).Kind != s_Kinds[index])
						{
							throw std::invalid_argument("Product cell has duplicate or incorrectly typed equipment");
						}
						ids[index] = id;
					}
				}
			}
			if (std::find(ids.begin(), ids.end(), ComponentId{0}) != ids.end())
			{
				throw std::invalid_argument("Install all eight product cell parts before starting its PLC");
			}
			return ids;
		}
	}

	std::vector<ComponentDefinition> GetProductCellParts()
	{
		std::vector<ComponentDefinition> parts(8);
		for (std::size_t index = 0; index < parts.size(); ++index)
		{
			parts[index].Name = s_Names[index];
			parts[index].Kind = s_Kinds[index];
		}
		parts[0].Properties.Speed = .28;
		parts[0].Properties.Acceleration = 1.8;
		parts[1].Placement.Position = {1.63, .69, .61};
		parts[1].Placement.Rotation.Y = -1.5707963267948966;
		parts[1].Properties.Speed = 12.0;
		parts[1].Properties.Acceleration = 48.0;
		parts[2].Placement.Position = {-1.15, .80, 0};
		parts[2].Properties.Mass = .35;
		for (std::size_t index = 3; index < 6; ++index)
		{
			parts[index].Placement.Position = {index == 3 ? -.85 : index == 4 ? -.10 : 1.30, .80, .42};
			parts[index].Properties.Threshold = index == 3 ? .30 : index == 4 ? 1.05 : 2.45;
			parts[index].Properties.Hysteresis = .015;
		}
		parts[6].Placement.Position = {0, 1.80, 0};
		parts[6].Placement.Rotation.Z = -1.5707963267948966;
		parts[6].Properties.Stroke = .22;
		parts[6].Properties.Speed = .22;
		parts[7].Placement.Position = {0, .80, 0};
		parts[7].Properties.Delay = .15;
		return parts;
	}

	std::vector<Connection> GetProductCellConnections(const Machine& machine)
	{
		const auto ids = Resolve(machine);
		return {{{ids[1], "Active"}, {ids[0], "Enable"}}, {{ids[0], "Velocity"}, {ids[2], "Velocity"}},
			{{ids[2], "Position"}, {ids[3], "Position"}}, {{ids[2], "Position"}, {ids[4], "Position"}},
			{{ids[2], "Position"}, {ids[5], "Position"}}};
	}

	ProductCellController::ProductCellController(const Machine& machine) : m_Parts(Resolve(machine))
	{
		const auto expected = GetProductCellConnections(machine);
		for (const auto& connection : expected)
		{
			if (std::find(machine.GetConnections().begin(), machine.GetConnections().end(), connection) == machine.GetConnections().end())
			{
				throw std::invalid_argument("Wire all five product cell signals before starting its PLC");
			}
		}
		for (const auto& connection : machine.GetConnections())
		{
			const auto& endpoint = connection.Destination;
			if ((endpoint.Component == m_Parts[1] && (endpoint.Signal == "Enable" || endpoint.Signal == "Reverse")) ||
				(endpoint.Component == m_Parts[2] && (endpoint.Signal == "Enable" || endpoint.Signal == "Processed")) ||
				(endpoint.Component == m_Parts[6] && (endpoint.Signal == "Extend" || endpoint.Signal == "Retract")) ||
				(endpoint.Component == m_Parts[7] && endpoint.Signal == "Enable") ||
				(endpoint.Component == m_Parts[0] && endpoint.Signal == "Reverse"))
			{
				throw std::invalid_argument("Simulated PLC output inputs must not have another driver");
			}
		}
	}

	void ProductCellController::Transition(ProductCellState state)
	{
		m_State = state;
		m_StateTime = 0.0;
	}

	void ProductCellController::Scan(const Machine& machine)
	{
		auto boolean = [&](std::size_t part, const char* signal) { return std::get<bool>(machine.GetOutput({m_Parts[part], signal})); };
		m_IO.Entry = boolean(3, "Detected");
		m_IO.Station = boolean(4, "Detected");
		m_IO.Exit = boolean(5, "Detected");
		m_IO.Clamped = boolean(7, "Active");
		m_IO.Extended = boolean(6, "Extended");
		m_IO.Retracted = boolean(6, "Retracted");
		m_IO.Processed = boolean(2, "Processed");
		m_StateTime += machine.GetTimeStep();
		++m_Scans;
		if (m_State != ProductCellState::Faulted)
		{
			if (machine.IsEmergencyStopped())
			{
				m_FaultReason = "Emergency stop; explicit Stop and restart required";
			}
			for (auto id : m_Parts)
			{
				if (machine.GetState(id).Fault != FaultCode::None)
				{
					m_FaultReason = "Equipment fault; clear cause and restart";
				}
			}
			if (m_StateTime > 20.0 && m_State != ProductCellState::Complete)
			{
				m_FaultReason = "PLC sequence timeout";
			}
			if ((m_IO.Station && !m_IO.Entry) || (m_IO.Exit && !m_IO.Station))
			{
				m_FaultReason = "Invalid sensor sequence";
			}
			if (!m_FaultReason.empty())
			{
				Transition(ProductCellState::Faulted);
			}
		}
		const bool stopped =
			std::abs(machine.GetState(m_Parts[0]).Velocity) < 1.e-10 && std::abs(machine.GetState(m_Parts[1]).Velocity) < 1.e-10;
		switch (m_State)
		{
			case ProductCellState::Feeding:
				if (m_IO.Entry)
				{
					Transition(ProductCellState::Locating);
				}
				break;
			case ProductCellState::Locating:
				if (m_IO.Station)
				{
					Transition(ProductCellState::Stopping);
				}
				break;
			case ProductCellState::Stopping:
				if (stopped)
				{
					Transition(ProductCellState::Clamping);
				}
				break;
			case ProductCellState::Clamping:
				if (m_IO.Clamped)
				{
					Transition(ProductCellState::Extending);
				}
				break;
			case ProductCellState::Extending:
				if (m_IO.Extended)
				{
					Transition(ProductCellState::Holding);
				}
				break;
			case ProductCellState::Holding:
				if (m_StateTime >= .45)
				{
					m_Marked = true;
					Transition(ProductCellState::Retracting);
				}
				break;
			case ProductCellState::Retracting:
				if (m_IO.Retracted)
				{
					Transition(ProductCellState::Releasing);
				}
				break;
			case ProductCellState::Releasing:
				if (!m_IO.Clamped)
				{
					Transition(ProductCellState::Discharging);
				}
				break;
			case ProductCellState::Discharging:
				if (m_IO.Exit)
				{
					Transition(ProductCellState::OutfeedStopping);
				}
				break;
			case ProductCellState::OutfeedStopping:
				if (stopped && m_IO.Processed)
				{
					m_CompletedProducts = 1;
					Transition(ProductCellState::Complete);
				}
				break;
			case ProductCellState::Complete:
			case ProductCellState::Faulted:
				break;
		}
		m_IO.Drive =
			m_State == ProductCellState::Feeding || m_State == ProductCellState::Locating || m_State == ProductCellState::Discharging;
		m_IO.Clamp = m_State == ProductCellState::Clamping || m_State == ProductCellState::Extending ||
					 m_State == ProductCellState::Holding || m_State == ProductCellState::Retracting;
		m_IO.Extend = m_State == ProductCellState::Extending || m_State == ProductCellState::Holding;
		m_IO.Retract = m_State == ProductCellState::Retracting;
	}

	void ProductCellController::Apply(Machine& machine)
	{
		auto next = *this;
		next.Scan(machine);
		auto replacement = machine;
		replacement.SetInput({m_Parts[1], "Enable"}, next.m_IO.Drive);
		replacement.SetInput({m_Parts[1], "Reverse"}, false);
		replacement.SetInput({m_Parts[0], "Reverse"}, false);
		replacement.SetInput({m_Parts[2], "Enable"}, true);
		replacement.SetInput({m_Parts[2], "Processed"}, next.m_Marked);
		replacement.SetInput({m_Parts[6], "Extend"}, next.m_IO.Extend);
		replacement.SetInput({m_Parts[6], "Retract"}, next.m_IO.Retract);
		replacement.SetInput({m_Parts[7], "Enable"}, next.m_IO.Clamp);
		machine = std::move(replacement);
		*this = std::move(next);
	}

	std::string_view ToString(ProductCellState state)
	{
		constexpr const char* names[]{"Feeding", "Locating product", "Stopping belt", "Clamping", "Head extending", "Processing",
			"Head retracting", "Releasing clamp", "Discharging", "Stopping outfeed", "Complete", "Faulted"};
		const auto index = static_cast<unsigned>(state);
		if (index >= std::size(names))
		{
			throw std::invalid_argument("Unknown product cell state");
		}
		return names[index];
	}
}