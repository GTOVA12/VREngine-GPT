#pragma once

#include "EditorUI.h"
#include "FactoryCore/EditorFiles.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

namespace FactoryCore
{
	// Sends real ImGui mouse events and verifies the state after each button click.
	class EditorGuiSmoke
	{
	public:
		EditorGuiSmoke(EditorUI& ui, EditorSession& session, std::filesystem::path machinePath)
			: m_UI(ui), m_Session(session), m_Path(std::move(machinePath)) {}

		void BeforeFrame(donut::app::DeviceManager& manager)
		{
			manager.RenderNextFrameWhileUnfocused();
			constexpr const char* controls[]{ "Add", "Duplicate", "Delete", "Undo", "Redo", "Play", "Pause", "Step", "Stop", "Save", "New", "Open", "Add", "New", "Cancel", "New", "Discard changes", "Open" };
			const unsigned action = m_Frame / 6;
			const unsigned phase = m_Frame % 6;
			if (action < std::size(controls) && (phase == 2 || phase == 3))
			{
				auto position = m_UI.GetControlPosition(controls[action]);
				if (!position) throw std::runtime_error("UI test control is not visible: " + std::string(controls[action]));
				ImGui::GetIO().AddMousePosEvent(position->x, position->y);
				ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, phase == 2);
			}
			if (action < std::size(controls) && phase == 5)
			{
				const auto count = m_Session.GetMachine().GetComponentIds().size();
				const std::size_t expectedCounts[]{ 7, 8, 7, 8, 7, 7, 7, 7, 7, 7, 0, 7, 8, 8, 8, 8, 0, 7 };
				if (count != expectedCounts[action]) throw std::runtime_error("UI test equipment count failed after " + std::string(controls[action]));
				if (action == 5 && !m_Session.IsPlaying()) throw std::runtime_error("UI Play did not start");
				if (action == 6)
				{
					if (m_Session.IsPlaying()) throw std::runtime_error("UI Pause did not pause");
					m_PausedTick = m_Session.GetMachine().GetTick();
				}
				if (action == 7 && m_Session.GetMachine().GetTick() != m_PausedTick + 1) throw std::runtime_error("UI Step did not advance exactly one tick");
				if (action == 8 && (!m_Session.IsEditing() || m_Session.GetMachine().GetTick() != 0)) throw std::runtime_error("UI Stop did not restore authored state");
				if (action == 9 && (m_Session.IsModified() || LoadEditorMachine(m_Path).GetComponentIds().size() != 7)) throw std::runtime_error("UI Save failed");
				std::cout << "PASS editor UI " << controls[action] << '\n';
			}
			if (++m_Frame == std::size(controls) * 6 + 1)
			{
				m_Complete = true;
				glfwSetWindowShouldClose(manager.GetWindow(), GLFW_TRUE);
			}
		}
		bool IsComplete() const { return m_Complete; }
	private:
		EditorUI& m_UI;
		EditorSession& m_Session;
		std::filesystem::path m_Path;
		unsigned m_Frame = 0;
		std::uint64_t m_PausedTick = 0;
		bool m_Complete = false;
	};
}
