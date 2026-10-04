#include "FactoryCore/Machine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace FactoryCore
{
	namespace
	{
		constexpr std::size_t s_MaxComponents = 10000;
		constexpr double s_MaxCoordinate = 1.0e12;
		constexpr double s_MaxProperty = 1.0e6;

		void Require(bool condition, const char* message)
		{
			if (!condition)
				throw std::invalid_argument(message);
		}

		bool IsFiniteVector(const Vector3& value)
		{
			return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z)
				&& std::abs(value.X) <= s_MaxCoordinate && std::abs(value.Y) <= s_MaxCoordinate
				&& std::abs(value.Z) <= s_MaxCoordinate;
		}

		SignalType FindSignal(const std::vector<SignalDescriptor>& signals, std::string_view name)
		{
			for (const auto& signal : signals)
			{
				if (signal.Name == name)
					return signal.Type;
			}
			throw std::invalid_argument("Unknown signal name");
		}

		SignalValue DefaultValue(SignalType type)
		{
			if (type == SignalType::Boolean)
				return false;
			return 0.0;
		}
	}

	std::string_view ToString(EquipmentKind kind)
	{
		switch (kind)
		{
			case EquipmentKind::Cylinder: return "Cylinder";
			case EquipmentKind::Sensor: return "Sensor";
			case EquipmentKind::Actuator: return "Actuator";
			case EquipmentKind::Motor: return "Motor";
			case EquipmentKind::Conveyor: return "Conveyor";
			case EquipmentKind::Workpiece: return "Workpiece";
		}
		throw std::invalid_argument("Unknown equipment kind");
	}

	EquipmentKind ParseEquipmentKind(std::string_view name)
	{
		for (const auto kind : { EquipmentKind::Cylinder, EquipmentKind::Sensor, EquipmentKind::Actuator,
			EquipmentKind::Motor, EquipmentKind::Conveyor, EquipmentKind::Workpiece })
		{
			if (ToString(kind) == name)
				return kind;
		}
		throw std::invalid_argument("Unknown equipment kind");
	}

	std::vector<SignalDescriptor> Machine::GetInputs(EquipmentKind kind)
	{
		std::vector<SignalDescriptor> signals{ { "Fault", SignalType::Boolean } };
		switch (kind)
		{
			case EquipmentKind::Cylinder:
				signals.push_back({ "Extend", SignalType::Boolean });
				signals.push_back({ "Retract", SignalType::Boolean });
				break;
			case EquipmentKind::Sensor:
				signals.push_back({ "Position", SignalType::Number });
				break;
			case EquipmentKind::Actuator:
				signals.push_back({ "Enable", SignalType::Boolean });
				break;
			case EquipmentKind::Motor:
			case EquipmentKind::Conveyor:
				signals.push_back({ "Enable", SignalType::Boolean });
				signals.push_back({ "Reverse", SignalType::Boolean });
				break;
			case EquipmentKind::Workpiece:
				signals.push_back({ "Enable", SignalType::Boolean });
				signals.push_back({ "Velocity", SignalType::Number });
				break;
			default: throw std::invalid_argument("Unknown equipment kind");
		}
		return signals;
	}

	std::vector<SignalDescriptor> Machine::GetOutputs(EquipmentKind kind)
	{
		std::vector<SignalDescriptor> signals{
			{ "Fault", SignalType::Boolean }, { "Active", SignalType::Boolean },
			{ "Position", SignalType::Number }, { "Velocity", SignalType::Number } };
		switch (kind)
		{
			case EquipmentKind::Cylinder:
				signals.push_back({ "Extended", SignalType::Boolean });
				signals.push_back({ "Retracted", SignalType::Boolean });
				break;
			case EquipmentKind::Sensor:
				signals.push_back({ "Detected", SignalType::Boolean });
				break;
			case EquipmentKind::Actuator:
			case EquipmentKind::Motor:
			case EquipmentKind::Conveyor:
			case EquipmentKind::Workpiece:
				break;
			default: throw std::invalid_argument("Unknown equipment kind");
		}
		return signals;
	}

	void Machine::Validate(const ComponentDefinition& definition)
	{
		Require(!definition.Name.empty() && definition.Name.size() <= 256
			&& definition.Name.find('\0') == std::string::npos, "Invalid component name");
		Require(definition.VisualModel.size() <= 4096
			&& definition.VisualModel.find('\0') == std::string::npos, "Invalid visual model path");
		(void)GetInputs(definition.Kind);
		const auto& transform = definition.Placement;
		Require(IsFiniteVector(transform.Position) && IsFiniteVector(transform.Rotation)
			&& IsFiniteVector(transform.Scale), "Transform must contain finite bounded numbers");
		Require(transform.Scale.X > 0.0 && transform.Scale.Y > 0.0 && transform.Scale.Z > 0.0,
			"Scale must be positive");
		const auto& properties = definition.Properties;
		for (const double value : { properties.Stroke, properties.Speed, properties.Acceleration,
			properties.Delay, properties.Threshold, properties.Hysteresis, properties.InitialPosition, properties.Mass })
			Require(std::isfinite(value) && std::abs(value) <= s_MaxProperty, "Properties must be finite and bounded");
		Require(properties.Stroke > 0.0 && properties.Speed > 0.0 && properties.Acceleration > 0.0
			&& properties.Mass > 0.0, "Stroke, speed, acceleration and mass must be positive");
		Require(properties.Delay >= 0.0 && properties.Hysteresis >= 0.0, "Delay and hysteresis cannot be negative");
		if (definition.Kind == EquipmentKind::Cylinder)
			Require(properties.InitialPosition >= 0.0 && properties.InitialPosition <= properties.Stroke,
				"Cylinder initial position must lie within its stroke");
	}

	Machine::Machine(std::string name, double timeStep)
		: m_Name(std::move(name)), m_TimeStep(timeStep)
	{
		Require(!m_Name.empty() && m_Name.size() <= 256 && m_Name.find('\0') == std::string::npos,
			"Invalid machine name");
		Require(std::isfinite(timeStep) && timeStep >= 1.0e-6 && timeStep <= 1.0,
			"Time step must be between one microsecond and one second");
	}

	Machine::Component Machine::MakeComponent(ComponentDefinition definition)
	{
		Component component;
		component.Definition = std::move(definition);
		component.State.Position = component.Definition.Properties.InitialPosition;
		for (const auto& signal : GetInputs(component.Definition.Kind))
			component.Inputs.emplace(signal.Name, DefaultValue(signal.Type));
		if (component.Definition.Kind == EquipmentKind::Sensor)
		{
			component.Inputs.at("Position") = component.State.Position;
			component.State.Active = component.State.Position >= component.Definition.Properties.Threshold;
		}
		if (component.Definition.Kind == EquipmentKind::Workpiece)
		{
			component.Inputs.at("Enable") = true;
			component.State.Active = true;
		}
		return component;
	}

	void Machine::Insert(ComponentDefinition definition)
	{
		Validate(definition);
		Require(definition.Id != 0 && definition.Id != std::numeric_limits<ComponentId>::max(), "Invalid component ID");
		Require(m_Components.size() < s_MaxComponents, "Component limit exceeded");
		Require(!m_Components.contains(definition.Id), "Duplicate component ID");
		const auto id = definition.Id;
		m_Components.emplace(id, MakeComponent(std::move(definition)));
		m_NextId = std::max(m_NextId, id + 1);
	}

	ComponentId Machine::Create(ComponentDefinition definition)
	{
		Require(definition.Id == 0, "Create assigns the component ID; use FromConfiguration to restore IDs");
		const auto id = m_NextId;
		definition.Id = id;
		Insert(std::move(definition));
		return id;
	}

	Machine::Component& Machine::Find(ComponentId id)
	{
		const auto iterator = m_Components.find(id);
		if (iterator == m_Components.end())
			throw std::out_of_range("Unknown component ID");
		return iterator->second;
	}

	const Machine::Component& Machine::Find(ComponentId id) const
	{
		const auto iterator = m_Components.find(id);
		if (iterator == m_Components.end())
			throw std::out_of_range("Unknown component ID");
		return iterator->second;
	}

	const ComponentDefinition& Machine::GetDefinition(ComponentId id) const { return Find(id).Definition; }
	const ComponentState& Machine::GetState(ComponentId id) const { return Find(id).State; }

	std::vector<ComponentId> Machine::GetComponentIds() const
	{
		std::vector<ComponentId> ids;
		ids.reserve(m_Components.size());
		for (const auto& [id, component] : m_Components)
		{
			(void)component;
			ids.push_back(id);
		}
		return ids;
	}

	ComponentId Machine::Duplicate(ComponentId id, std::string name)
	{
		auto definition = GetDefinition(id);
		definition.Id = 0;
		definition.Name = std::move(name);
		return Create(std::move(definition));
	}

	void Machine::Remove(ComponentId id)
	{
		(void)Find(id);
		for (const auto& connection : m_Connections)
		{
			if (connection.Source.Component == id && connection.Destination.Component != id)
			{
				auto& target = Find(connection.Destination.Component);
				const auto type = FindSignal(GetInputs(target.Definition.Kind), connection.Destination.Signal);
				target.Inputs.at(connection.Destination.Signal) = DefaultValue(type);
			}
		}
		std::erase_if(m_Connections, [id](const Connection& connection)
		{
			return connection.Source.Component == id || connection.Destination.Component == id;
		});
		m_Components.erase(id);
	}

	void Machine::Configure(ComponentId id, ComponentDefinition definition)
	{
		auto& component = Find(id);
		Require(definition.Id == 0 || definition.Id == id, "Cannot change a component ID");
		Require(definition.Kind == component.Definition.Kind, "Cannot change equipment kind in place");
		definition.Id = id;
		Validate(definition);
		component = MakeComponent(std::move(definition));
	}

	void Machine::Place(ComponentId id, Transform placement)
	{
		auto& component = Find(id);
		auto definition = component.Definition;
		definition.Placement = placement;
		Validate(definition);
		component.Definition.Placement = placement;
	}

	bool Machine::IsConnected(const Endpoint& destination) const
	{
		return std::any_of(m_Connections.begin(), m_Connections.end(), [&destination](const Connection& connection)
		{
			return connection.Destination == destination;
		});
	}

	void Machine::SetInput(const Endpoint& endpoint, SignalValue value)
	{
		auto& component = Find(endpoint.Component);
		const auto type = FindSignal(GetInputs(component.Definition.Kind), endpoint.Signal);
		Require((type == SignalType::Boolean) == std::holds_alternative<bool>(value), "Signal type mismatch");
		if (const auto* number = std::get_if<double>(&value))
			Require(std::isfinite(*number) && std::abs(*number) <= s_MaxCoordinate, "Signal value must be finite and bounded");
		Require(!IsConnected(endpoint), "Connected inputs cannot have an external driver");
		component.Inputs.at(endpoint.Signal) = value;
	}

	SignalValue Machine::GetInput(const Endpoint& endpoint) const
	{
		const auto& component = Find(endpoint.Component);
		(void)FindSignal(GetInputs(component.Definition.Kind), endpoint.Signal);
		return component.Inputs.at(endpoint.Signal);
	}

	SignalValue Machine::GetOutput(const Endpoint& endpoint) const
	{
		const auto& component = Find(endpoint.Component);
		(void)FindSignal(GetOutputs(component.Definition.Kind), endpoint.Signal);
		const auto& state = component.State;
		if (endpoint.Signal == "Fault") return state.Fault != FaultCode::None;
		if (endpoint.Signal == "Active" || endpoint.Signal == "Detected") return state.Active;
		if (endpoint.Signal == "Position") return state.Position;
		if (endpoint.Signal == "Velocity") return state.Velocity;
		if (endpoint.Signal == "Extended") return state.Position >= component.Definition.Properties.Stroke;
		if (endpoint.Signal == "Retracted") return state.Position <= 0.0;
		throw std::logic_error("Output descriptor has no implementation");
	}

	void Machine::Connect(Connection connection)
	{
		const auto& source = Find(connection.Source.Component);
		const auto& destination = Find(connection.Destination.Component);
		const auto sourceType = FindSignal(GetOutputs(source.Definition.Kind), connection.Source.Signal);
		const auto destinationType = FindSignal(GetInputs(destination.Definition.Kind), connection.Destination.Signal);
		Require(sourceType == destinationType, "Cannot connect different signal types");
		Require(!IsConnected(connection.Destination), "Input already has a connection");
		m_Connections.push_back(std::move(connection));
	}

	void Machine::Disconnect(const Endpoint& destination)
	{
		auto& component = Find(destination.Component);
		const auto type = FindSignal(GetInputs(component.Definition.Kind), destination.Signal);
		Require(IsConnected(destination), "Input has no connection");
		std::erase_if(m_Connections, [&destination](const Connection& connection)
		{
			return connection.Destination == destination;
		});
		component.Inputs.at(destination.Signal) = DefaultValue(type);
	}

	void Machine::Update(Component& component)
	{
		auto& state = component.State;
		const auto& properties = component.Definition.Properties;
		const auto boolean = [&component](const char* name) { return std::get<bool>(component.Inputs.at(name)); };
		const auto number = [&component](const char* name) { return std::get<double>(component.Inputs.at(name)); };
		if (boolean("Fault"))
			state.Fault = FaultCode::External;
		if (component.Definition.Kind == EquipmentKind::Cylinder && boolean("Extend") && boolean("Retract"))
			state.Fault = FaultCode::ConflictingCommands;
		if (m_EmergencyStop || state.Fault != FaultCode::None)
		{
			state.Velocity = 0.0;
			state.Active = false;
			state.TransitionTime = 0.0;
			return;
		}

		switch (component.Definition.Kind)
		{
			case EquipmentKind::Cylinder:
			{
				const double oldPosition = state.Position;
				const double direction = boolean("Extend") ? 1.0 : (boolean("Retract") ? -1.0 : 0.0);
				state.Position = std::clamp(oldPosition + direction * properties.Speed * m_TimeStep, 0.0, properties.Stroke);
				// Snap floating-point endpoint error so discrete end sensors always settle.
				const double tolerance = properties.Stroke * 1.0e-12;
				if (state.Position < tolerance) state.Position = 0.0;
				if (properties.Stroke - state.Position < tolerance) state.Position = properties.Stroke;
				state.Velocity = (state.Position - oldPosition) / m_TimeStep;
				state.Active = state.Velocity != 0.0;
				break;
			}
			case EquipmentKind::Sensor:
				state.Position = number("Position");
				state.Velocity = 0.0;
				state.Active = state.Active ? state.Position >= properties.Threshold - properties.Hysteresis
					: state.Position >= properties.Threshold;
				break;
			case EquipmentKind::Actuator:
				state.Velocity = 0.0;
				if (boolean("Enable") != state.Active)
				{
					state.TransitionTime += m_TimeStep;
					if (state.TransitionTime + 1.0e-12 >= properties.Delay)
					{
						state.Active = boolean("Enable");
						state.TransitionTime = 0.0;
					}
				}
				else
					state.TransitionTime = 0.0;
				state.Position = state.Active ? 1.0 : 0.0;
				break;
			case EquipmentKind::Motor:
			case EquipmentKind::Conveyor:
			{
				const double target = boolean("Enable") ? properties.Speed * (boolean("Reverse") ? -1.0 : 1.0) : 0.0;
				state.Velocity += std::clamp(target - state.Velocity,
					-properties.Acceleration * m_TimeStep, properties.Acceleration * m_TimeStep);
				state.Position += state.Velocity * m_TimeStep;
				state.Active = state.Velocity != 0.0;
				break;
			}
			case EquipmentKind::Workpiece:
				state.Active = boolean("Enable");
				state.Velocity = state.Active ? number("Velocity") : 0.0;
				state.Position += state.Velocity * m_TimeStep;
				break;
		}
		if (!std::isfinite(state.Position) || std::abs(state.Position) > s_MaxCoordinate)
		{
			state.Position = std::clamp(state.Position, -s_MaxCoordinate, s_MaxCoordinate);
			state.Velocity = 0.0;
			state.Active = false;
			state.Fault = FaultCode::External;
		}
	}

	void Machine::Step()
	{
		if (m_Tick == std::numeric_limits<std::uint64_t>::max())
			throw std::overflow_error("Simulation tick overflow");
		// Snapshot all outputs before modifying inputs or states, including feedback loops.
		std::vector<SignalValue> sampled;
		sampled.reserve(m_Connections.size());
		for (const auto& connection : m_Connections)
			sampled.push_back(GetOutput(connection.Source));
		for (std::size_t index = 0; index < m_Connections.size(); ++index)
		{
			const auto& destination = m_Connections[index].Destination;
			Find(destination.Component).Inputs.at(destination.Signal) = sampled[index];
		}
		for (auto& [id, component] : m_Components)
		{
			(void)id;
			Update(component);
		}
		++m_Tick;
	}

	void Machine::ResetFault(ComponentId id)
	{
		auto& component = Find(id);
		Require(!std::get<bool>(component.Inputs.at("Fault")), "Clear external fault before resetting");
		if (component.Definition.Kind == EquipmentKind::Cylinder)
			Require(!std::get<bool>(component.Inputs.at("Extend")) && !std::get<bool>(component.Inputs.at("Retract")),
				"Clear cylinder commands before resetting");
		component.State.Fault = FaultCode::None;
	}

	void Machine::Reset()
	{
		auto replacement = FromConfiguration(GetConfiguration());
		replacement.m_NextId = m_NextId;
		*this = std::move(replacement);
	}

	MachineDefinition Machine::GetConfiguration() const
	{
		MachineDefinition definition;
		definition.Name = m_Name;
		definition.TimeStep = m_TimeStep;
		for (const auto& [id, component] : m_Components)
		{
			(void)id;
			definition.Components.push_back(component.Definition);
		}
		definition.Connections = m_Connections;
		return definition;
	}

	Machine Machine::FromConfiguration(const MachineDefinition& definition)
	{
		Machine machine(definition.Name, definition.TimeStep);
		for (const auto& component : definition.Components)
			machine.Insert(component);
		for (const auto& connection : definition.Connections)
			machine.Connect(connection);
		return machine;
	}

	void Machine::ReplaceConfiguration(const MachineDefinition& definition)
	{
		auto replacement = FromConfiguration(definition);
		*this = std::move(replacement);
	}

	std::vector<ComponentId> Machine::SpawnAssembly(const MachineDefinition& assembly, Vector3 offset)
	{
		Require(IsFiniteVector(offset), "Invalid assembly offset");
		const auto validated = FromConfiguration(assembly);
		auto replacement = *this;
		std::map<ComponentId, ComponentId> remapping;
		std::vector<ComponentId> spawned;
		for (const auto id : validated.GetComponentIds())
		{
			auto definition = validated.GetDefinition(id);
			definition.Id = 0;
			definition.Placement.Position.X += offset.X;
			definition.Placement.Position.Y += offset.Y;
			definition.Placement.Position.Z += offset.Z;
			const auto newId = replacement.Create(std::move(definition));
			remapping.emplace(id, newId);
			spawned.push_back(newId);
		}
		for (const auto& connection : validated.GetConnections())
		{
			replacement.Connect({ { remapping.at(connection.Source.Component), connection.Source.Signal },
				{ remapping.at(connection.Destination.Component), connection.Destination.Signal } });
		}
		*this = std::move(replacement);
		return spawned;
	}
}
