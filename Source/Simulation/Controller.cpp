#include "FactoryCore/Controller.h"

#include <stdexcept>

namespace FactoryCore
{
	CylinderCycleController::CylinderCycleController(ComponentId cylinder, std::uint64_t cycles)
		: m_Cylinder(cylinder), m_TargetCycles(cycles)
	{
		if (cylinder == 0 || cycles == 0)
			throw std::invalid_argument("A cylinder and positive cycle count are required");
	}

	void CylinderCycleController::Start()
	{
		if (m_State == CycleState::Extending || m_State == CycleState::Retracting)
			throw std::logic_error("Controller is already running");
		m_CompletedCycles = 0;
		m_State = CycleState::Extending;
	}

	void CylinderCycleController::Apply(Machine& machine)
	{
		if (machine.GetDefinition(m_Cylinder).Kind != EquipmentKind::Cylinder)
			throw std::invalid_argument("Cycle controller requires a cylinder");
		for (const auto& connection : machine.GetConnections())
		{
			if (connection.Destination.Component == m_Cylinder
				&& (connection.Destination.Signal == "Extend" || connection.Destination.Signal == "Retract"))
				throw std::invalid_argument("Cycle controller command inputs must not be wired");
		}
		if (machine.IsEmergencyStopped() || std::get<bool>(machine.GetOutput({ m_Cylinder, "Fault" })))
			m_State = CycleState::Faulted;
		if (m_State == CycleState::Extending && std::get<bool>(machine.GetOutput({ m_Cylinder, "Extended" })))
			m_State = CycleState::Retracting;
		if (m_State == CycleState::Retracting && std::get<bool>(machine.GetOutput({ m_Cylinder, "Retracted" })))
		{
			++m_CompletedCycles;
			m_State = m_CompletedCycles >= m_TargetCycles ? CycleState::Complete : CycleState::Extending;
		}
		// Controller owns both command inputs. Wired commands are rejected by Machine.
		machine.SetInput({ m_Cylinder, "Extend" }, m_State == CycleState::Extending);
		machine.SetInput({ m_Cylinder, "Retract" }, m_State == CycleState::Retracting);
	}
}
