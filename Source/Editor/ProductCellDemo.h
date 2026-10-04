#pragma once
#include "EditorUI.h"
#include "FactoryCore/EditorFiles.h"
#include "FactoryCore/ProductCellAssembly.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace FactoryCore
{
	// Repeatable demonstration/test. Assembly and wiring use visible editor buttons.
	// Simulation uses an explicit .05 s frame clock (five .01 s PLC scans).
	class ProductCellDemo
	{
	  public:
		ProductCellDemo(
			EditorUI& ui, EditorSession& session, SceneRenderer& renderer, std::filesystem::path machinePath, std::filesystem::path capture)
			: m_UI(ui), m_Session(session), m_Renderer(renderer), m_Path(std::move(machinePath)), m_Capture(std::move(capture)),
			  m_Directory(m_Capture.parent_path())
		{
			m_Renderer.SetAutomaticSimulation(false);
			m_Trace.open(m_Directory / "PLC-Trace.csv");
			if (!m_Trace)
			{
				throw std::runtime_error("Cannot write PLC trace");
			}
			m_Trace << "tick,time,state,productPosition,headPosition,processed,entry,station,exit,clamped,drive,extend,retract\n";
		}

		void BeforeFrame(donut::app::DeviceManager& manager)
		{
			manager.RenderNextFrameWhileUnfocused();
			constexpr unsigned assemblyActions = 17;
			if (m_Frame < assemblyActions * 6)
			{
				const unsigned action = m_Frame / 6, phase = m_Frame % 6;
				const char* control = action < 8	 ? "Add next cell part"
									  : action < 13	 ? "Wire next cell signal"
									  : action == 13 ? "Frame cell"
									  : action == 14 ? "Save"
									  : action == 15 ? "Use cell PLC"
													 : "Play";
				if (phase == 2 || phase == 3)
				{
					Click(control, phase == 2);
				}
				if (phase == 5)
				{
					if (action < 8)
					{
						const auto ids = m_Session.GetMachine().GetComponentIds();
						Require(ids.size() == action + 1, "An editor assembly click did not install its part");
						const auto expected = GetProductCellVisualParts(m_Renderer.GetAssetsDirectory())[action];
						const auto& actual = m_Session.GetMachine().GetDefinition(ids.back());
						Require(actual.Name == expected.Name && actual.Kind == expected.Kind &&
									actual.Placement.Position.X == expected.Placement.Position.X &&
									actual.Placement.Position.Y == expected.Placement.Position.Y &&
									actual.Placement.Position.Z == expected.Placement.Position.Z &&
									actual.Placement.Rotation.X == expected.Placement.Rotation.X &&
									actual.Placement.Rotation.Y == expected.Placement.Rotation.Y &&
									actual.Placement.Rotation.Z == expected.Placement.Rotation.Z &&
									actual.Placement.Scale.X == expected.Placement.Scale.X &&
									actual.Placement.Scale.Y == expected.Placement.Scale.Y &&
									actual.Placement.Scale.Z == expected.Placement.Scale.Z &&
									actual.Properties.Stroke == expected.Properties.Stroke &&
									actual.Properties.Speed == expected.Properties.Speed &&
									actual.Properties.Acceleration == expected.Properties.Acceleration &&
									actual.Properties.Delay == expected.Properties.Delay &&
									actual.Properties.Threshold == expected.Properties.Threshold &&
									actual.Properties.Hysteresis == expected.Properties.Hysteresis &&
									actual.Properties.InitialPosition == expected.Properties.InitialPosition &&
									actual.Properties.Mass == expected.Properties.Mass && actual.VisualModel == expected.VisualModel,
							"Editor installed an incorrect equipment definition");
						std::cout << "PASS editor assembled " << actual.Name << '\n';
					}
					if (action >= 8 && action < 13)
					{
						Require(m_Session.GetMachine().GetConnections().size() == action - 7, "Editor wiring click failed");
					}
					if (action == 14)
					{
						const auto saved = LoadEditorMachine(m_Path);
						Require(saved.GetComponentIds().size() == 8 && saved.GetConnections().size() == 5 && !m_Session.IsModified(),
							"Editor save/reload failed");
						ProductCellController validated(saved);
						m_CaptureAssembly = true;
						std::cout << "PASS editor saved and reloaded complete wired machine\n";
					}
					if (action == 16)
					{
						Require(m_Session.IsPlaying() && m_Session.GetProductCellPLC(), "Play did not start the cell PLC");
					}
				}
			}
			else if (!m_CompletedCycle)
			{
				Require(m_Session.IsPlaying(), "Cell stopped before finishing its product");
				for (unsigned scan = 0; scan < 5; ++scan)
				{
					m_Session.Advance(.01);
					const auto* plc = m_Session.GetProductCellPLC();
					Require(plc && plc->GetState() != ProductCellState::Faulted, "Product PLC faulted");
					const auto& machine = m_Session.GetMachine();
					const auto& io = plc->GetIO();
					m_Trace << machine.GetTick() << ',' << machine.GetTime() << ',' << ToString(plc->GetState()) << ','
							<< machine.GetState(3).Position << ',' << machine.GetState(7).Position << ',' << machine.GetState(3).Processed
							<< ',' << io.Entry << ',' << io.Station << ',' << io.Exit << ',' << io.Clamped << ',' << io.Drive << ','
							<< io.Extend << ',' << io.Retract << '\n';
					if (plc->GetState() == ProductCellState::Holding)
					{
						Require(io.Clamped && io.Extended && std::abs(machine.GetState(3).Velocity) < 1.e-12,
							"Unsecured product during processing");
						if (!m_ProcessingCaptured)
						{
							m_CaptureProcessing = true;
						}
					}
					if (plc->GetState() == ProductCellState::Complete)
					{
						Require(plc->GetCompletedProducts() == 1 && machine.GetState(3).Processed && machine.GetState(3).Position > 2.45 &&
									machine.GetState(1).Velocity == 0.0 && machine.GetState(2).Velocity == 0.0 && io.Retracted &&
									!io.Clamped,
							"Invalid finished product");
						Require(m_ProcessingCaptured, "No processing capture was recorded");
						m_CompletedCycle = true;
						m_CaptureComplete = true;
						m_StopFrame = 0;
						std::cout << "PASS editor PLC processed and discharged one product in " << machine.GetTime() << " s / "
								  << plc->GetScans() << " scans\n";
						break;
					}
				}
				if (m_Session.GetMachine().GetTick() > 2500)
				{
					throw std::runtime_error("Editor PLC test exceeded its tick budget");
				}
			}
			else
			{
				if (m_StopFrame == 2 || m_StopFrame == 3)
				{
					Click("Stop", m_StopFrame == 2);
				}
				if (m_StopFrame == 5)
				{
					Require(m_Session.IsEditing() && m_Session.GetMachine().GetTick() == 0 &&
								!m_Session.GetMachine().GetState(3).Processed && !m_Session.IsModified(),
						"Stop did not restore the authored cell");
					m_Trace.close();
					Require(static_cast<bool>(m_Trace), "PLC trace write failed");
					m_Complete = true;
					glfwSetWindowShouldClose(manager.GetWindow(), GLFW_TRUE);
					std::cout << "PASS editor Stop restored unprocessed authored machine\n";
				}
				++m_StopFrame;
			}
			++m_Frame;
		}

		void AfterRender(donut::app::DeviceManager& manager)
		{
			if (m_CaptureAssembly)
			{

				const auto& buffer = manager.GetCurrentBackBuffer()->getDesc();
				const auto point = m_Renderer.GetProjectionMatrix() * m_Renderer.GetViewMatrix() * glm::vec4(-1.15f, .90f, 0.f, 1.f);
				Require(point.w > 0.f && m_Renderer.Pick((point.x / point.w + 1.f) * static_cast<float>(buffer.width) * .5f,
											 (1.f - point.y / point.w) * static_cast<float>(buffer.height) * .5f) == 3,
					"Supersampled editor picking did not match the rendered product");
				m_Renderer.Capture(m_Directory / "Editor-Assembled.png", manager.GetCurrentBackBuffer());
				CaptureCellImage(m_Directory / "01-Assembled-Cell.png");
				m_CaptureAssembly = false;
			}
			if (m_CaptureProcessing)
			{
				m_Renderer.Capture(m_Directory / "Editor-Processing.png", manager.GetCurrentBackBuffer());
				CaptureCellImage(m_Directory / "02-Processing-Cell.png");
				m_Renderer.SetCamera({0.f, 1.05f, 0.f}, -.65f, .30f, 2.6f);
				m_Renderer.CaptureQuality(m_Directory / "03-Process-Detail.png");
				FrameCell();
				m_CaptureProcessing = false;
				m_ProcessingCaptured = true;
			}
			if (m_CaptureComplete)
			{
				m_Renderer.Capture(m_Capture, manager.GetCurrentBackBuffer());
				CaptureCellImage(m_Directory / "04-Completed-Cell.png");
				const auto x = static_cast<float>(m_Session.GetMachine().GetState(3).Position - 1.15);
				m_Renderer.SetCamera({x, .88f, 0.f}, -.6f, .55f, 1.25f);
				m_Renderer.CaptureQuality(m_Directory / "05-Inspected-Product.png");
				FrameCell();
				m_CaptureComplete = false;
			}
		}

		bool IsComplete() const
		{
			return m_Complete;
		}

	  private:
		static void Require(bool condition, const char* message)
		{
			if (!condition)
			{
				std::cerr << message << std::endl;
				throw std::runtime_error(message);
			}
		}
		void FrameCell()
		{
			m_Renderer.SetCamera({0.f, .90f, 0.f}, -.72f, .35f, 5.8f);
		}

		void CaptureCellImage(const std::filesystem::path& path)
		{
			m_Renderer.SetCamera({0.f, .95f, 0.f}, -.72f, .52f, 4.4f);
			m_Renderer.CaptureQuality(path);
			FrameCell();
		}
		void Click(const char* control, bool down)
		{
			const auto position = m_UI.GetControlPosition(control);
			if (!position)
			{
				throw std::runtime_error("Cell test control is not visible: " + std::string(control));
			}
			ImGui::GetIO().AddMousePosEvent(position->x, position->y);
			ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down);
		}
		EditorUI& m_UI;
		EditorSession& m_Session;
		SceneRenderer& m_Renderer;
		std::filesystem::path m_Path;
		std::filesystem::path m_Capture;
		std::filesystem::path m_Directory;
		std::ofstream m_Trace;
		unsigned m_Frame = 0;
		unsigned m_StopFrame = 0;
		bool m_CaptureAssembly = false;
		bool m_CaptureProcessing = false;
		bool m_ProcessingCaptured = false;
		bool m_CaptureComplete = false;
		bool m_CompletedCycle = false;
		bool m_Complete = false;
	};
}