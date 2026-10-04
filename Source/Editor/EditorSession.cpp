#include "FactoryCore/EditorSession.h"
#include "FactoryCore/Persistence.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace FactoryCore
{
	namespace
	{
		constexpr std::size_t s_HistoryLimit = 100;
		constexpr int s_MaxTicksPerFrame = 256;

		std::string Encode(const MachineDefinition& definition)
		{
			std::ostringstream output;
			SaveConfiguration(Machine::FromConfiguration(definition), output);
			return output.str();
		}
	}

	EditorSession::EditorSession(Machine machine) : m_Machine(std::move(machine)) { MarkSaved(); }

	bool EditorSession::IsModified() const { return Encode(GetAuthoredConfiguration()) != m_SavedConfiguration; }
	void EditorSession::MarkSaved() { m_SavedConfiguration = Encode(GetAuthoredConfiguration()); }
	std::optional<CycleState> EditorSession::GetCycleState() const
	{
		auto cycle = dynamic_cast<const CylinderCycleController*>(m_Controller.get());
		return cycle ? std::optional<CycleState>(cycle->GetState()) : std::nullopt;
	}

	const ProductCellController* EditorSession::GetProductCellPLC() const { return dynamic_cast<const ProductCellController*>(m_Controller.get()); }

	void EditorSession::RequireEditing() const
	{
		if (!IsEditing())
			throw std::logic_error("Stop simulation before editing the machine");
	}

	void EditorSession::ValidateSelection()
	{
		const auto ids = m_Machine.GetComponentIds();
		if (std::find(ids.begin(), ids.end(), m_Selection) == ids.end())
			m_Selection = 0;
	}

	void EditorSession::Select(ComponentId id)
	{
		if (id != 0) (void)m_Machine.GetDefinition(id);
		m_Selection = id;
	}

	void EditorSession::Load(Machine machine)
	{
		RequireEditing();
		if (m_Transaction)
			throw std::logic_error("Finish the current edit before loading");
		const auto savedConfiguration = Encode(machine.GetConfiguration());
		m_Machine = std::move(machine);
		m_SavedConfiguration = savedConfiguration;
		m_Undo.clear();
		m_Redo.clear();
		m_Selection = 0;
		m_LastError.clear();
		++m_Revision;
	}

	void EditorSession::Remember(MachineDefinition definition)
	{
		m_Undo.push_back(std::move(definition));
		if (m_Undo.size() > s_HistoryLimit)
			m_Undo.erase(m_Undo.begin());
		m_Redo.clear();
	}

	void EditorSession::Edit(const std::function<void(Machine&)>& operation)
	{
		RequireEditing();
		auto replacement = m_Machine;
		operation(replacement);
		if (!m_Transaction)
			Remember(m_Machine.GetConfiguration());
		m_Machine = std::move(replacement);
		ValidateSelection();
		++m_Revision;
	}

	void EditorSession::BeginTransaction()
	{
		RequireEditing();
		if (m_Transaction) throw std::logic_error("An edit transaction is already open");
		m_Transaction = m_Machine.GetConfiguration();
	}

	void EditorSession::CommitTransaction()
	{
		RequireEditing();
		if (!m_Transaction) throw std::logic_error("No edit transaction is open");
		if (Encode(*m_Transaction) != Encode(m_Machine.GetConfiguration()))
			Remember(*m_Transaction);
		m_Transaction.reset();
	}

	void EditorSession::CancelTransaction()
	{
		RequireEditing();
		if (!m_Transaction) throw std::logic_error("No edit transaction is open");
		m_Machine.ReplaceConfiguration(*m_Transaction);
		m_Transaction.reset();
		ValidateSelection();
		++m_Revision;
	}

	bool EditorSession::CanUndo() const { return IsEditing() && !m_Transaction && !m_Undo.empty(); }
	bool EditorSession::CanRedo() const { return IsEditing() && !m_Transaction && !m_Redo.empty(); }

	void EditorSession::Undo()
	{
		if (!CanUndo()) throw std::logic_error("Undo is unavailable");
		auto replacement = Machine::FromConfiguration(m_Undo.back());
		m_Redo.push_back(m_Machine.GetConfiguration());
		m_Machine = std::move(replacement);
		m_Undo.pop_back();
		ValidateSelection();
		++m_Revision;
	}

	void EditorSession::Redo()
	{
		if (!CanRedo()) throw std::logic_error("Redo is unavailable");
		auto replacement = Machine::FromConfiguration(m_Redo.back());
		m_Undo.push_back(m_Machine.GetConfiguration());
		m_Machine = std::move(replacement);
		m_Redo.pop_back();
		ValidateSelection();
		++m_Revision;
	}

	void EditorSession::Play(std::unique_ptr<IController> controller)
	{
		if (m_Transaction) throw std::logic_error("Finish the current edit before playing");
		if (!m_LastError.empty()) throw std::logic_error("Stop simulation to recover from its error");
		if (m_Playing) throw std::logic_error("Simulation is already playing");
		if (!m_PlaybackConfiguration)
		{
			m_PlaybackConfiguration = m_Machine.GetConfiguration();
			m_Machine.Reset();
			m_Controller = std::move(controller);
			m_Accumulator = 0.0;
		}
		else if (controller)
			throw std::logic_error("Stop before replacing the active controller");
		m_Playing = true;
	}

	void EditorSession::Pause() { m_Playing = false; }

	void EditorSession::Stop()
	{
		if (m_PlaybackConfiguration)
		{
			m_Machine.ReplaceConfiguration(*m_PlaybackConfiguration);
			m_PlaybackConfiguration.reset();
			++m_Revision;
		}
		m_Controller.reset();
		m_Playing = false;
		m_Accumulator = 0.0;
		m_LastError.clear();
		ValidateSelection();
	}

	void EditorSession::Tick()
	{
		try
		{
			if (m_Controller) m_Controller->Apply(m_Machine);
			m_Machine.Step();
			ValidateSelection();
		}
		catch (const std::exception& error)
		{
			m_Playing = false;
			m_LastError = error.what();
			throw;
		}
	}

	void EditorSession::StepOnce()
	{
		if (m_Transaction) throw std::logic_error("Finish the current edit before stepping");
		if (m_Playing) throw std::logic_error("Pause before stepping");
		if (!m_LastError.empty()) throw std::logic_error("Stop simulation to recover from its error");
		if (!m_PlaybackConfiguration)
		{
			m_PlaybackConfiguration = m_Machine.GetConfiguration();
			m_Machine.Reset();
		}
		Tick();
	}

	void EditorSession::Advance(double elapsedSeconds)
	{
		if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0 || elapsedSeconds > 1.0)
			throw std::invalid_argument("Frame elapsed time must be finite and between zero and one second");
		if (!m_Playing) return;
		m_Accumulator += elapsedSeconds;
		const double timeStep = m_Machine.GetTimeStep();
		int ticks = 0;
		while (m_Accumulator + timeStep * 1.0e-9 >= timeStep && ticks < s_MaxTicksPerFrame)
		{
			Tick();
			m_Accumulator = std::max(0.0, m_Accumulator - timeStep);
			++ticks;
		}
		if (m_Accumulator >= timeStep)
		{
			m_Playing = false;
			m_LastError = "Simulation cannot keep up; stopped playback. Stop and reduce the simulation rate.";
		}
	}

	MachineDefinition EditorSession::GetAuthoredConfiguration() const
	{
		return m_PlaybackConfiguration ? *m_PlaybackConfiguration : m_Machine.GetConfiguration();
	}

	void EditorSession::SetInput(Endpoint endpoint, SignalValue value) { m_Machine.SetInput(endpoint, value); }
	void EditorSession::SetEmergencyStop(bool active) { m_Machine.SetEmergencyStop(active); }
	void EditorSession::ResetFault(ComponentId id) { m_Machine.ResetFault(id); }
}
