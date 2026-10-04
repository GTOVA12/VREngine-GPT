#pragma once
#include "SceneRenderer.h"
#include <donut/app/imgui_renderer.h>
#include <array>
#include <string>
#include <map>
#include <optional>

namespace FactoryCore
{
	class EditorUI final : public donut::app::ImGui_Renderer
	{
	public:
		EditorUI(donut::app::DeviceManager* manager, EditorSession& session, SceneRenderer& renderer, std::string machinePath = {});
		void RequestClose();
		std::optional<glm::vec2> GetControlPosition(const std::string& label) const;
	private:
		void buildUI() override;
		bool Button(const char* label);
		void ConfirmChanges();
		void PerformPending();
		void Toolbar();
		void EquipmentList();
		void ProductCellPanel();
		void Inspector();
		void Wiring();
		void Gizmo();
		void Attempt(const std::function<void()>& operation);
		EditorSession& m_Session;
		SceneRenderer& m_Renderer;
		std::string m_Error;
		std::array<char, 1024> m_MachinePath{};
		std::array<char, 1024> m_AssetPath{};
		std::array<char, 1024> m_PrefabPath{};
		std::array<char, 1024> m_ScriptPath{};
		std::array<char, 1024> m_EnvironmentPath{};
		std::array<char, 1024> m_MaterialPath{};
		int m_ControlMode = 1;
		int m_GizmoOperation = 0;
		int m_GizmoMode = 0;
		int m_CreateKind = 0;
		ComponentId m_WireSource = 0;
		ComponentId m_WireDestination = 0;
		int m_SourceSignal = 0;
		int m_DestinationSignal = 0;
		bool m_GizmoWasUsing = false;
		bool m_EmergencyStop = false;
		ComponentId m_MaterialSelection = 0;
		int m_MaterialChoice = 0;
		MaterialInfo m_Material;
		std::map<std::string, glm::vec2> m_Controls;
		int m_PendingAction = 0;
		bool m_ConfirmChanges = false;
		std::string m_CurrentMachinePath;
		std::string m_PendingPath;
	};
}
