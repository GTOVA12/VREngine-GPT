#pragma once

#include "FactoryCore/Machine.h"

namespace FactoryCore
{
	// Adapters apply commands before Machine::Step; no rendering or transport dependency.
	class IController
	{
	public:
		virtual ~IController() = default;
		virtual void Apply(Machine& machine) = 0;
	};

	enum class CycleState { Idle, Extending, Retracting, Complete, Faulted };

	class CylinderCycleController final : public IController
	{
	public:
		explicit CylinderCycleController(ComponentId cylinder, std::uint64_t cycles = 1);
		void Start();
		void Apply(Machine& machine) override;
		CycleState GetState() const { return m_State; }
		std::uint64_t GetCompletedCycles() const { return m_CompletedCycles; }

	private:
		ComponentId m_Cylinder;
		std::uint64_t m_TargetCycles;
		std::uint64_t m_CompletedCycles = 0;
		CycleState m_State = CycleState::Idle;
	};
}
