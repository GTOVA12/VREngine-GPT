#pragma once

#include "FactoryCore/Controller.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace FactoryCore
{
	// Graphics-independent authoring, undo/redo and fixed-step playback owner.
	class EditorSession
	{
	public:
		explicit EditorSession(Machine machine = Machine{});
		const Machine& GetMachine() const { return m_Machine; }
		void Load(Machine machine);
		void Edit(const std::function<void(Machine&)>& operation);
		void BeginTransaction();
		void CommitTransaction();
		void CancelTransaction();
		void Undo();
		void Redo();
		bool CanUndo() const;
		bool CanRedo() const;
		bool IsEditing() const { return !m_PlaybackConfiguration.has_value(); }
		bool IsModified() const;
		void MarkSaved();
		std::optional<CycleState> GetCycleState() const;
		bool HasTransaction() const { return m_Transaction.has_value(); }
		std::uint64_t GetRevision() const { return m_Revision; }

		void Select(ComponentId id);
		ComponentId GetSelection() const { return m_Selection; }

		void Play(std::unique_ptr<IController> controller = {});
		void Pause();
		void Stop();
		void StepOnce();
		void Advance(double elapsedSeconds);
		bool IsPlaying() const { return m_Playing; }
		const std::string& GetLastError() const { return m_LastError; }
		MachineDefinition GetAuthoredConfiguration() const;

		void SetInput(Endpoint endpoint, SignalValue value);
		void SetEmergencyStop(bool active);
		void ResetFault(ComponentId id);

	private:
		void RequireEditing() const;
		void ValidateSelection();
		void Tick();
		void Remember(MachineDefinition definition);

		Machine m_Machine;
		std::vector<MachineDefinition> m_Undo;
		std::vector<MachineDefinition> m_Redo;
		std::optional<MachineDefinition> m_Transaction;
		std::optional<MachineDefinition> m_PlaybackConfiguration;
		std::unique_ptr<IController> m_Controller;
		ComponentId m_Selection = 0;
		std::uint64_t m_Revision = 0;
		bool m_Playing = false;
		double m_Accumulator = 0.0;
		std::string m_LastError;
		std::string m_SavedConfiguration;
	};
}
