#include "EditorUI.h"
#include "FactoryCore/Persistence.h"
#include "FactoryCore/EditorFiles.h"
#include <GLFW/glfw3.h>
#ifdef FACTORYCORE_WITH_LUA
#include "FactoryCore/ScriptController.h"
#endif

#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace FactoryCore
{
	namespace
	{
		template<std::size_t Size>
		void Text(std::array<char, Size>& buffer, const std::string& text)
		{
			buffer.fill(0);
			std::copy_n(text.data(), std::min(text.size(), Size - 1), buffer.data());
		}

		bool EditVector(const char* label, Vector3& vector, bool degrees = false)
		{
			float data[]{ static_cast<float>(vector.X), static_cast<float>(vector.Y), static_cast<float>(vector.Z) };
			if (degrees) for (auto& value : data) value = glm::degrees(value);
			if (!ImGui::DragFloat3(label, data, 0.01f)) return false;
			if (degrees) for (auto& value : data) value = glm::radians(value);
			vector = { data[0], data[1], data[2] };
			return true;
		}

		void ComponentCombo(const char* label, const Machine& machine, ComponentId& selection)
		{
			const auto ids = machine.GetComponentIds();
			if (std::find(ids.begin(), ids.end(), selection) == ids.end()) selection = ids.empty() ? 0 : ids.front();
			if (ImGui::BeginCombo(label, selection ? machine.GetDefinition(selection).Name.c_str() : "No equipment"))
			{
				for (auto id : ids)
				{
					const auto name = std::to_string(id) + ": " + machine.GetDefinition(id).Name;
					if (ImGui::Selectable(name.c_str(), selection == id)) selection = id;
				}
				ImGui::EndCombo();
			}
		}

		void SignalCombo(const char* label, const std::vector<SignalDescriptor>& signals, int& selection)
		{
			if (signals.empty()) return;
			selection = std::clamp(selection, 0, static_cast<int>(signals.size()) - 1);
			if (ImGui::BeginCombo(label, signals[static_cast<std::size_t>(selection)].Name.c_str()))
			{
				for (int index = 0; index < static_cast<int>(signals.size()); ++index)
					if (ImGui::Selectable(signals[static_cast<std::size_t>(index)].Name.c_str(), selection == index)) selection = index;
				ImGui::EndCombo();
			}
		}
	}

	EditorUI::EditorUI(donut::app::DeviceManager* manager, EditorSession& session, SceneRenderer& renderer, std::string machinePath)
		: ImGui_Renderer(manager), m_Session(session), m_Renderer(renderer)
	{
		Text(m_MachinePath, machinePath.empty() ? "Machine.factory" : machinePath);
		m_CurrentMachinePath = m_MachinePath.data();
		Text(m_PrefabPath, (m_Renderer.GetAssetsDirectory().parent_path() / "Examples/CylinderCell.factory").string());
		Text(m_MaterialPath, "EditedAssets/Material.gltf");
		Text(m_ScriptPath, (m_Renderer.GetAssetsDirectory().parent_path() / "Examples/CylinderCycle.lua").string());
		ImGui::GetIO().IniFilename = nullptr;
		ImGui::StyleColorsDark();
		auto& style = ImGui::GetStyle();
		style.WindowRounding = 6.f;
		style.FrameRounding = 3.f;
	}

	void EditorUI::Attempt(const std::function<void()>& operation)
	{
		try { operation(); m_Error.clear(); }
		catch (const std::exception& error) { m_Error = error.what(); }
	}

	bool EditorUI::Button(const char* label)
	{
		const bool pressed = ImGui::Button(label);
		const auto minimum = ImGui::GetItemRectMin();
		const auto maximum = ImGui::GetItemRectMax();
		m_Controls[label] = { (minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f };
		return pressed;
	}

	std::optional<glm::vec2> EditorUI::GetControlPosition(const std::string& label) const
	{
		auto found = m_Controls.find(label);
		return found == m_Controls.end() ? std::nullopt : std::optional<glm::vec2>(found->second);
	}

	void EditorUI::RequestClose()
	{
		Attempt([&]
		{
			m_PendingAction = 3;
			if (m_Session.IsModified()) m_ConfirmChanges = true;
			else PerformPending();
		});
	}

	void EditorUI::PerformPending()
	{
		if (m_PendingAction == 1)
		{
			m_Session.Load(LoadEditorMachine(m_PendingPath));
			m_CurrentMachinePath = m_PendingPath;
			Text(m_MachinePath, m_CurrentMachinePath);
		}
		if (m_PendingAction == 2) m_Session.Load(Machine{});
		if (m_PendingAction == 3) glfwSetWindowShouldClose(GetDeviceManager()->GetWindow(), GLFW_TRUE);
		m_PendingAction = 0;
		m_ConfirmChanges = false;
	}

	void EditorUI::ConfirmChanges()
	{
		if (m_ConfirmChanges) { ImGui::OpenPopup("Unsaved machine"); m_ConfirmChanges = false; }
		if (ImGui::BeginPopupModal("Unsaved machine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Save your machine before continuing?");
			ImGui::InputText("Machine file", m_MachinePath.data(), m_MachinePath.size());
			if (Button("Save and continue")) Attempt([&]
			{
				SaveEditorMachine(m_Session.GetAuthoredConfiguration(), m_MachinePath.data());
				m_Session.MarkSaved();
				m_CurrentMachinePath = m_MachinePath.data();
				PerformPending();
				ImGui::CloseCurrentPopup();
			});
			ImGui::SameLine();
			if (Button("Discard changes")) Attempt([&] { PerformPending(); ImGui::CloseCurrentPopup(); });
			ImGui::SameLine();
			if (Button("Cancel")) { m_PendingAction = 0; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	void EditorUI::Toolbar()
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 88));
		ImGui::Begin("FactoryCore", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImGui::TextUnformatted("FACTORYCORE");
		ImGui::SameLine();
		ImGui::Text("| %s | tick %llu | %.2f s", m_Session.GetMachine().GetName().c_str(),
			static_cast<unsigned long long>(m_Session.GetMachine().GetTick()), m_Session.GetMachine().GetTime());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(300);
		ImGui::InputText("##Machine file", m_MachinePath.data(), m_MachinePath.size());
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
		if (Button("Open")) Attempt([&] { m_PendingAction = 1;
			m_PendingPath = m_MachinePath.data();
			if (m_Session.IsModified()) { Text(m_MachinePath, m_CurrentMachinePath); m_ConfirmChanges = true; }
			else PerformPending(); });
		ImGui::SameLine();
		if (Button("New")) Attempt([&] { m_PendingAction = 2; if (m_Session.IsModified()) m_ConfirmChanges = true; else PerformPending(); });
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(m_Session.HasTransaction());
		if (Button("Save")) Attempt([&] { SaveEditorMachine(m_Session.GetAuthoredConfiguration(), m_MachinePath.data()); m_Session.MarkSaved();
				m_CurrentMachinePath = m_MachinePath.data(); });
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Session.CanUndo());
		if (Button("Undo")) Attempt([&] { m_Session.Undo(); });
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Session.CanRedo());
		if (Button("Redo")) Attempt([&] { m_Session.Redo(); });
		ImGui::EndDisabled();

		ImGui::BeginDisabled(m_Session.HasTransaction() || m_Session.IsPlaying());
		if (Button(m_Session.IsEditing() ? "Play" : "Resume"))
			Attempt([&]
			{
				if (!m_Session.IsEditing()) { m_Session.Play(); return; }
				std::unique_ptr<IController> controller;
				if (m_ControlMode == 1)
				{
					ComponentId cylinder = 0;
					for (auto id : m_Session.GetMachine().GetComponentIds())
						if (m_Session.GetMachine().GetDefinition(id).Kind == EquipmentKind::Cylinder) { cylinder = id; break; }
					if (!cylinder) throw std::invalid_argument("Create a cylinder to run the cycle controller");
					auto cycle = std::make_unique<CylinderCycleController>(cylinder, 1000000);
					cycle->Start();
					controller = std::move(cycle);
				}
#ifdef FACTORYCORE_WITH_LUA
				if (m_ControlMode == 2)
				{
					auto script = std::make_unique<ScriptController>(ReadScriptFile(m_ScriptPath.data()));
					if (m_PrefabPath[0] && std::filesystem::is_regular_file(m_PrefabPath.data()))
						script->RegisterPrefab("Assembly", LoadEditorMachine(m_PrefabPath.data()).GetConfiguration());
					controller = std::move(script);
				}
#endif
				m_Session.Play(std::move(controller));
			});
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (Button("Pause")) m_Session.Pause();
		ImGui::SameLine();
		if (Button("Stop")) Attempt([&] { m_Session.Stop(); m_EmergencyStop = false; });
		ImGui::SameLine();
		ImGui::BeginDisabled(m_Session.IsPlaying() || m_Session.HasTransaction());
		if (Button("Step")) Attempt([&] { m_Session.StepOnce(); });
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Checkbox("Emergency stop", &m_EmergencyStop)) m_Session.SetEmergencyStop(m_EmergencyStop);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(190);
		ImGui::BeginDisabled(!m_Session.IsEditing());
#ifdef FACTORYCORE_WITH_LUA
		ImGui::Combo("Control", &m_ControlMode, "Manual\0Cylinder cycle\0Lua script\0");
#else
		ImGui::Combo("Control", &m_ControlMode, "Manual\0Cylinder cycle\0");
#endif
		ImGui::EndDisabled();
		ImGui::End();
	}

	void EditorUI::EquipmentList()
	{
		const auto height = ImGui::GetIO().DisplaySize.y;
		ImGui::SetNextWindowPos(ImVec2(8, 96));
		ImGui::SetNextWindowSize(ImVec2(280, height - 134));
		ImGui::Begin("Machine assembly", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
		ImGui::SetNextItemWidth(155);
		ImGui::Combo("##Equipment kind", &m_CreateKind, "Cylinder\0Sensor\0Actuator\0Motor\0Conveyor\0Workpiece\0");
		ImGui::SameLine();
		if (Button("Add"))
			Attempt([&]
			{
				ComponentId created = 0;
				m_Session.Edit([&](Machine& machine)
				{
					ComponentDefinition definition;
					definition.Kind = static_cast<EquipmentKind>(m_CreateKind);
					definition.Name = std::string(ToString(definition.Kind));
					if (m_Session.GetSelection()) definition.Placement = machine.GetDefinition(m_Session.GetSelection()).Placement;
					definition.Placement.Position.X += .3;
					created = machine.Create(definition);
				});
				m_Session.Select(created);
			});
		ImGui::EndDisabled();
		ImGui::Separator();
		const auto ids = m_Session.GetMachine().GetComponentIds();
		for (auto id : ids)
		{
			const auto& definition = m_Session.GetMachine().GetDefinition(id);
			const auto& state = m_Session.GetMachine().GetState(id);
			const auto label = std::to_string(id) + "  " + definition.Name + (state.Fault == FaultCode::None ? "" : " [FAULT]");
			if (ImGui::Selectable(label.c_str(), id == m_Session.GetSelection()) && !m_Session.HasTransaction()) m_Session.Select(id);
		}
		ImGui::BeginDisabled(!m_Session.IsEditing() || !m_Session.GetSelection() || m_Session.HasTransaction());
		if (Button("Duplicate")) Attempt([&]
		{
			ComponentId duplicate = 0;
			m_Session.Edit([&](Machine& machine) { auto id = m_Session.GetSelection(); duplicate = machine.Duplicate(id, machine.GetDefinition(id).Name + " copy"); });
			m_Session.Select(duplicate);
		});
		ImGui::SameLine();
		if (Button("Delete")) Attempt([&] { m_Session.Edit([&](Machine& machine) { machine.Remove(m_Session.GetSelection()); }); });
		ImGui::EndDisabled();
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Reusable assembly"))
		{
			ImGui::InputText("Prefab file", m_PrefabPath.data(), m_PrefabPath.size());
			ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
			if (Button("Spawn assembly")) Attempt([&]
			{
				const auto prefab = LoadEditorMachine(m_PrefabPath.data()).GetConfiguration();
				m_Session.Edit([&](Machine& machine) { (void)machine.SpawnAssembly(prefab); });
			});
			ImGui::SameLine();
			if (Button("Save assembly")) Attempt([&]
			{
				SaveEditorMachine(m_Session.GetAuthoredConfiguration(), m_PrefabPath.data());
			});
			ImGui::EndDisabled();
		}
		if (ImGui::CollapsingHeader("Signal wiring")) Wiring();
		if (ImGui::CollapsingHeader("Lua control"))
		{
			ImGui::InputText("Script file", m_ScriptPath.data(), m_ScriptPath.size());
			ImGui::TextWrapped("Select Lua script in Control, then Play. The assembly file is registered as prefab 'Assembly'.");
		}
		if (ImGui::CollapsingHeader("Machine settings"))
		{
			auto definition = m_Session.GetAuthoredConfiguration();
			std::array<char, 1024> name{};
			Text(name, definition.Name);
			ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
			bool changed = ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue);
			changed |= ImGui::InputDouble("Tick seconds", &definition.TimeStep, .001, .01, "%.6f", ImGuiInputTextFlags_None);
			if (changed) Attempt([&] { definition.Name = name.data(); m_Session.Edit([&](Machine& machine) { machine.ReplaceConfiguration(definition); }); });
			ImGui::EndDisabled();
		}
		ImGui::End();
	}

	void EditorUI::Wiring()
	{
		const auto& machine = m_Session.GetMachine();
		ComponentCombo("From", machine, m_WireSource);
		ComponentCombo("To", machine, m_WireDestination);
		ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
		if (m_WireSource && m_WireDestination)
		{
			const auto outputs = Machine::GetOutputs(machine.GetDefinition(m_WireSource).Kind);
			const auto inputs = Machine::GetInputs(machine.GetDefinition(m_WireDestination).Kind);
			SignalCombo("Output", outputs, m_SourceSignal);
			SignalCombo("Input", inputs, m_DestinationSignal);
			if (!outputs.empty() && !inputs.empty() && Button("Connect"))
				Attempt([&]
				{
					const Connection connection{ { m_WireSource, outputs[static_cast<std::size_t>(m_SourceSignal)].Name },
						{ m_WireDestination, inputs[static_cast<std::size_t>(m_DestinationSignal)].Name } };
					m_Session.Edit([&](Machine& replacement) { replacement.Connect(connection); });
				});
		}
		const auto connections = machine.GetConnections();
		int index = 0;
		for (const auto& connection : connections)
		{
			ImGui::PushID(index++);
			ImGui::TextWrapped("%llu.%s -> %llu.%s", static_cast<unsigned long long>(connection.Source.Component), connection.Source.Signal.c_str(),
				static_cast<unsigned long long>(connection.Destination.Component), connection.Destination.Signal.c_str());
			if (ImGui::SmallButton("Disconnect")) Attempt([&] { m_Session.Edit([&](Machine& replacement) { replacement.Disconnect(connection.Destination); }); });
			ImGui::PopID();
		}
		ImGui::EndDisabled();
	}

	void EditorUI::Inspector()
	{
		const auto size = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(size.x - 328, 96));
		ImGui::SetNextWindowSize(ImVec2(320, size.y - 134));
		ImGui::Begin("Equipment inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		ImGui::PushItemWidth(150.f);
		const auto id = m_Session.GetSelection();
		if (id)
		{
			auto definition = m_Session.GetMachine().GetDefinition(id);
			ImGui::Text("%s | ID %llu", ToString(definition.Kind).data(), static_cast<unsigned long long>(id));
			if (Button("Focus in viewport")) m_Renderer.Focus(id);
			std::array<char, 1024> name{};
			Text(name, definition.Name);
			ImGui::BeginDisabled(!m_Session.IsEditing() || m_Session.HasTransaction());
			if (ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue))
				Attempt([&] { definition.Name = name.data(); m_Session.Edit([&](Machine& machine) { machine.Configure(id, definition); }); });
			auto placement = definition.Placement;
			bool placed = EditVector("Position", placement.Position);
			placed |= EditVector("Rotation", placement.Rotation, true);
			placed |= EditVector("Scale", placement.Scale);
			if (placed) Attempt([&] { m_Session.Edit([&](Machine& machine) { machine.Place(id, placement); }); });
			if (ImGui::CollapsingHeader("Equipment parameters", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& properties = definition.Properties;
				bool changed = false;
				const auto flags = ImGuiInputTextFlags_None;
				changed |= ImGui::InputDouble("Stroke", &properties.Stroke, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Speed", &properties.Speed, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Acceleration", &properties.Acceleration, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Delay", &properties.Delay, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Threshold", &properties.Threshold, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Hysteresis", &properties.Hysteresis, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Initial position", &properties.InitialPosition, .01, .1, "%.3f", flags);
				changed |= ImGui::InputDouble("Mass", &properties.Mass, .01, .1, "%.3f", flags);
				if (changed) Attempt([&] { m_Session.Edit([&](Machine& machine) { machine.Configure(id, definition); }); });
			}
			if (ImGui::CollapsingHeader("Visual model and PBR"))
			{
				ImGui::TextWrapped("Model: %s", definition.VisualModel.empty() ? "Equipment default" : definition.VisualModel.c_str());
				ImGui::InputText("glTF file", m_AssetPath.data(), m_AssetPath.size());
				if (Button("Import model")) Attempt([&]
				{
					auto path = std::filesystem::absolute(m_AssetPath.data());
					m_Renderer.CheckModel(path);
					definition.VisualModel = path.generic_string();
					m_Session.Edit([&](Machine& machine) { machine.Configure(id, definition); });
				});
				ImGui::SameLine();
				if (Button("Default model")) Attempt([&] { definition.VisualModel.clear(); m_Session.Edit([&](Machine& machine) { machine.Configure(id, definition); }); });
				const auto materials = m_Renderer.GetMaterials(id);
				if (!materials.empty())
				{
					if (m_MaterialSelection != id || m_MaterialChoice >= static_cast<int>(materials.size()))
					{
						m_MaterialSelection = id;
						m_MaterialChoice = 0;
						m_Material = materials.front();
					}
					if (ImGui::BeginCombo("Material", materials[static_cast<std::size_t>(m_MaterialChoice)].Name.c_str()))
					{
						for (int choice = 0; choice < static_cast<int>(materials.size()); ++choice)
							if (ImGui::Selectable(materials[static_cast<std::size_t>(choice)].Name.c_str(), choice == m_MaterialChoice))
							{
								m_MaterialChoice = choice;
								m_Material = materials[static_cast<std::size_t>(choice)];
							}
						ImGui::EndCombo();
					}
					ImGui::ColorEdit3("Base color", glm::value_ptr(m_Material.Color));
					ImGui::SliderFloat("Metalness", &m_Material.Metalness, 0.f, 1.f);
					ImGui::SliderFloat("Roughness", &m_Material.Roughness, .04f, 1.f);
					ImGui::InputText("New glTF file", m_MaterialPath.data(), m_MaterialPath.size());
					if (Button("Apply and save material")) Attempt([&]
					{
						definition.VisualModel = m_Renderer.ExportMaterial(id, m_Material, m_MaterialPath.data());
						m_Session.Edit([&](Machine& machine) { machine.Configure(id, definition); });
						m_MaterialSelection = 0;
					});
				}
			}
			ImGui::EndDisabled();
			const auto& state = m_Session.GetMachine().GetState(id);
			ImGui::SeparatorText("Live state");
			ImGui::Text("Position %.4f | velocity %.4f", state.Position, state.Velocity);
			ImGui::Text("State %s | fault %d", state.Active ? "Active" : "Inactive", static_cast<int>(state.Fault));
			if (auto cycle = m_Session.GetCycleState())
			{
				const char* states[]{ "Idle", "Extending", "Retracting", "Complete", "Faulted" };
				ImGui::Text("Cycle: %s", states[static_cast<int>(*cycle)]);
			}
			if (Button("Reset fault")) Attempt([&] { m_Session.ResetFault(id); });
			for (const auto& signal : Machine::GetOutputs(definition.Kind))
			{
				const auto value = m_Session.GetMachine().GetOutput({ id, signal.Name });
				if (signal.Type == SignalType::Boolean) ImGui::Text("%s: %s", signal.Name.c_str(), std::get<bool>(value) ? "true" : "false");
				else ImGui::Text("%s: %.4f", signal.Name.c_str(), std::get<double>(value));
			}
			ImGui::SeparatorText("Input commands");
			ImGui::BeginDisabled(m_Session.IsEditing());
			for (const auto& signal : Machine::GetInputs(definition.Kind))
			{
				const Endpoint endpoint{ id, signal.Name };
				bool driven = false;
				for (const auto& connection : m_Session.GetMachine().GetConnections())
					if (connection.Destination == endpoint) driven = true;
				ImGui::BeginDisabled(driven);
				auto value = m_Session.GetMachine().GetInput(endpoint);
				if (signal.Type == SignalType::Boolean)
				{
					auto active = std::get<bool>(value);
					if (ImGui::Checkbox(signal.Name.c_str(), &active)) Attempt([&] { m_Session.SetInput(endpoint, active); });
				}
				else
				{
					auto number = std::get<double>(value);
					if (ImGui::InputDouble(signal.Name.c_str(), &number, .01, .1, "%.4f", ImGuiInputTextFlags_None))
						Attempt([&] { m_Session.SetInput(endpoint, number); });
				}
				ImGui::EndDisabled();
			}
			ImGui::EndDisabled();
		}
		else ImGui::TextWrapped("Select equipment in the list or viewport.");

		if (ImGui::CollapsingHeader("Lighting and display"))
		{
			auto& settings = m_Renderer.GetSettings();
			ImGui::Checkbox("SSAO", &settings.AmbientOcclusion);
			ImGui::Checkbox("Soft shadows", &settings.Shadows);
			ImGui::Checkbox("HDRI lighting", &settings.ImageBasedLighting);
			ImGui::SliderFloat("Exposure EV", &settings.Exposure, -5.f, 5.f);
			ImGui::SliderFloat("AO radius", &settings.OcclusionRadius, .01f, 1.f);
			ImGui::SliderFloat("Sun angle", &settings.SunAngle, .1f, 5.f);
			ImGui::InputText("HDR environment", m_EnvironmentPath.data(), m_EnvironmentPath.size());
			if (Button("Load HDRI")) Attempt([&] { m_Renderer.SetEnvironment(m_EnvironmentPath.data()); });
		}
		ImGui::PopItemWidth();
		ImGui::End();
	}

	void EditorUI::Gizmo()
	{
		auto& io = ImGui::GetIO();
		ImGuizmo::BeginFrame();
		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
		const auto id = m_Session.GetSelection();
		if (id && m_Session.IsEditing())
		{
			auto placement = m_Session.GetMachine().GetDefinition(id).Placement;
			const auto& position = placement.Position;
			const auto& rotation = placement.Rotation;
			const auto& scale = placement.Scale;
			auto matrix = glm::translate(glm::mat4(1), glm::vec3(static_cast<float>(position.X), static_cast<float>(position.Y), static_cast<float>(position.Z)))
				* glm::mat4_cast(glm::quat(glm::vec3(static_cast<float>(rotation.X), static_cast<float>(rotation.Y), static_cast<float>(rotation.Z))))
				* glm::scale(glm::mat4(1), glm::vec3(static_cast<float>(scale.X), static_cast<float>(scale.Y), static_cast<float>(scale.Z)));
			const ImGuizmo::OPERATION operations[]{ ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE };
			const bool changed = ImGuizmo::Manipulate(glm::value_ptr(m_Renderer.GetViewMatrix()), glm::value_ptr(m_Renderer.GetProjectionMatrix()),
				operations[m_GizmoOperation], m_GizmoMode ? ImGuizmo::WORLD : ImGuizmo::LOCAL, glm::value_ptr(matrix));
			const bool usingGizmo = ImGuizmo::IsUsing();
			if (usingGizmo && !m_GizmoWasUsing) Attempt([&] { m_Session.BeginTransaction(); });
			if (changed) Attempt([&]
			{
				glm::vec3 translation, scaling, skew;
				glm::vec4 perspective;
				glm::quat quaternion;
				if (!glm::decompose(matrix, scaling, quaternion, translation, skew, perspective))
					throw std::invalid_argument("Gizmo transform cannot be decomposed");
				const auto euler = glm::eulerAngles(glm::normalize(quaternion));
				placement.Position = { translation.x, translation.y, translation.z };
				placement.Rotation = { euler.x, euler.y, euler.z };
				placement.Scale = { std::max(.001f, scaling.x), std::max(.001f, scaling.y), std::max(.001f, scaling.z) };
				m_Session.Edit([&](Machine& machine) { machine.Place(id, placement); });
			});
			if (!usingGizmo && m_GizmoWasUsing && m_Session.HasTransaction()) Attempt([&] { m_Session.CommitTransaction(); });
			m_GizmoWasUsing = usingGizmo;
		}
		const bool viewport = io.MousePos.x > 288 && io.MousePos.x < io.DisplaySize.x - 328 && io.MousePos.y > 96;
		if (viewport && !ImGuizmo::IsUsing() && !io.WantCaptureMouse)
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) m_Renderer.Orbit(io.MouseDelta.x, -io.MouseDelta.y);
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) m_Renderer.Pan(io.MouseDelta.x, io.MouseDelta.y);
			if (io.MouseWheel) m_Renderer.Zoom(io.MouseWheel);
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
				Attempt([&] { m_Session.Select(m_Renderer.Pick(io.MousePos.x, io.MousePos.y)); });
		}
		if (!io.WantTextInput && !m_Session.HasTransaction())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = 2;
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && m_Session.CanUndo()) Attempt([&] { m_Session.Undo(); });
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && m_Session.CanRedo()) Attempt([&] { m_Session.Redo(); });
			if (ImGui::IsKeyPressed(ImGuiKey_F) && id) m_Renderer.Focus(id);
		}
	}

	void EditorUI::buildUI()
	{
		m_Controls.clear();
		Toolbar();
		EquipmentList();
		Inspector();
		ConfirmChanges();
		Gizmo();
		const auto size = ImGui::GetIO().DisplaySize;
		ImGui::SetNextWindowPos(ImVec2(296, 96));
		ImGui::SetNextWindowSize(ImVec2(std::max(100.f, size.x - 632), 62));
		ImGui::Begin("Viewport tools", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		ImGui::SetNextItemWidth(135);
		ImGui::Combo("Gizmo", &m_GizmoOperation, "Move (W)\0Rotate (E)\0Scale (R)\0");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::Combo("Space", &m_GizmoMode, "Local\0World\0");
		ImGui::TextUnformatted("Right drag orbit | middle drag pan | wheel zoom | F focus");
		ImGui::End();
		ImGui::SetNextWindowPos(ImVec2(8, size.y - 32));
		ImGui::SetNextWindowSize(ImVec2(size.x - 16, 28));
		ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		const auto& error = !m_Error.empty() ? m_Error : !m_Session.GetLastError().empty() ? m_Session.GetLastError() : m_Renderer.GetLastError();
		if (!error.empty()) ImGui::TextColored(ImVec4(1.f, .4f, .35f, 1.f), "%s", error.c_str());
		else ImGui::TextUnformatted(m_Session.IsEditing() ? "Editing machine | Vulkan / PBR / HDR" : m_Session.IsPlaying() ? "Simulation running" : "Simulation paused");
		ImGui::End();
	}
}
