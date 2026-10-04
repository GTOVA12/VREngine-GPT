#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace FactoryCore
{
	using ComponentId = std::uint64_t;
	using SignalValue = std::variant<bool, double>;

	enum class EquipmentKind { Cylinder, Sensor, Actuator, Motor, Conveyor, Workpiece };
	enum class SignalType { Boolean, Number };
	enum class FaultCode { None, ConflictingCommands, External };

	struct Vector3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	struct Transform
	{
		Vector3 Position;
		Vector3 Rotation;
		Vector3 Scale{ 1.0, 1.0, 1.0 };
	};

	struct EquipmentProperties
	{
		double Stroke = 1.0;
		double Speed = 1.0;
		double Acceleration = 1.0;
		double Delay = 0.0;
		double Threshold = 0.5;
		double Hysteresis = 0.0;
		double InitialPosition = 0.0;
		double Mass = 1.0;
	};

	struct ComponentDefinition
	{
		ComponentId Id = 0;
		std::string Name;
		EquipmentKind Kind = EquipmentKind::Cylinder;
		Transform Placement;
		std::string VisualModel;
		EquipmentProperties Properties;
	};

	struct ComponentState
	{
		double Position = 0.0;
		double Velocity = 0.0;
		double TransitionTime = 0.0;
		bool Active = false;
		FaultCode Fault = FaultCode::None;
	};

	struct Endpoint
	{
		ComponentId Component = 0;
		std::string Signal;
		bool operator==(const Endpoint&) const = default;
	};

	struct Connection
	{
		Endpoint Source;
		Endpoint Destination;
		bool operator==(const Connection&) const = default;
	};

	struct MachineDefinition
	{
		std::string Name = "Machine";
		double TimeStep = 0.01;
		std::vector<ComponentDefinition> Components;
		std::vector<Connection> Connections;
	};

	struct SignalDescriptor
	{
		std::string Name;
		SignalType Type;
	};

	// Single-threaded owner: editor, controllers and adapters mutate between ticks.
	class Machine
	{
	public:
		explicit Machine(std::string name = "Machine", double timeStep = 0.01);

		ComponentId Create(ComponentDefinition definition);
		ComponentId Duplicate(ComponentId id, std::string name);
		void Remove(ComponentId id);
		void Configure(ComponentId id, ComponentDefinition definition);
		void Place(ComponentId id, Transform placement);
		std::vector<ComponentId> SpawnAssembly(const MachineDefinition& assembly, Vector3 offset = {});

		const ComponentDefinition& GetDefinition(ComponentId id) const;
		const ComponentState& GetState(ComponentId id) const;
		std::vector<ComponentId> GetComponentIds() const;
		const std::vector<Connection>& GetConnections() const { return m_Connections; }

		void SetInput(const Endpoint& endpoint, SignalValue value);
		SignalValue GetInput(const Endpoint& endpoint) const;
		SignalValue GetOutput(const Endpoint& endpoint) const;
		void Connect(Connection connection);
		void Disconnect(const Endpoint& destination);

		void Step();
		void ResetFault(ComponentId id);
		void Reset();
		void SetEmergencyStop(bool active) { m_EmergencyStop = active; }
		bool IsEmergencyStopped() const { return m_EmergencyStop; }
		std::uint64_t GetTick() const { return m_Tick; }
		double GetTimeStep() const { return m_TimeStep; }
		double GetTime() const { return static_cast<double>(m_Tick) * m_TimeStep; }
		const std::string& GetName() const { return m_Name; }

		MachineDefinition GetConfiguration() const;
		static Machine FromConfiguration(const MachineDefinition& definition);
		void ReplaceConfiguration(const MachineDefinition& definition);

		static std::vector<SignalDescriptor> GetInputs(EquipmentKind kind);
		static std::vector<SignalDescriptor> GetOutputs(EquipmentKind kind);
		static void Validate(const ComponentDefinition& definition);

	private:
		struct Component
		{
			ComponentDefinition Definition;
			ComponentState State;
			std::map<std::string, SignalValue, std::less<>> Inputs;
		};

		Component& Find(ComponentId id);
		const Component& Find(ComponentId id) const;
		void Insert(ComponentDefinition definition);
		static Component MakeComponent(ComponentDefinition definition);
		void Update(Component& component);
		bool IsConnected(const Endpoint& destination) const;

		std::string m_Name;
		double m_TimeStep;
		std::uint64_t m_Tick = 0;
		ComponentId m_NextId = 1;
		bool m_EmergencyStop = false;
		std::map<ComponentId, Component> m_Components;
		std::vector<Connection> m_Connections;
	};

	std::string_view ToString(EquipmentKind kind);
	EquipmentKind ParseEquipmentKind(std::string_view name);
}
