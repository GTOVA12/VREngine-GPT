#include "SceneRenderer.h"
#include "EditorUI.h"
#ifdef FACTORYCORE_GPU_TESTS
#include "EditorGuiSmoke.h"
#endif
#include "FactoryCore/Persistence.h"
#include "FactoryCore/EditorFiles.h"
#include <donut/core/log.h>
#include <donut/engine/TextureCache.h>
#include <chrono>
#include <charconv>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	class WindowCloseGuard
	{
	public:
		explicit WindowCloseGuard(GLFWwindow* window, FactoryCore::EditorUI& ui) : m_Window(window)
		{
			s_UI = &ui;
			m_Previous = glfwSetWindowCloseCallback(window, [](GLFWwindow* closingWindow)
			{
				glfwSetWindowShouldClose(closingWindow, GLFW_FALSE);
				s_UI->RequestClose();
			});
		}
		~WindowCloseGuard()
		{
			glfwSetWindowCloseCallback(m_Window, m_Previous);
			s_UI = nullptr;
		}
	private:
		GLFWwindow* m_Window;
		GLFWwindowclosefun m_Previous = nullptr;
		inline static FactoryCore::EditorUI* s_UI = nullptr;
	};

	class LogResetGuard
	{
	public:
		~LogResetGuard() { donut::log::ResetCallback(); }
	};

	class GraphicsMessages final : public nvrhi::IMessageCallback
	{
	public:
		std::atomic<int> Errors{ 0 };
		void message(nvrhi::MessageSeverity severity, const char* text) override
		{
			if (severity >= nvrhi::MessageSeverity::Error) ++Errors;
			std::cerr << "Graphics: " << text << '\n';
		}
	};

	FactoryCore::Machine Demo()
	{
		using namespace FactoryCore;
		Machine machine("FactoryCore demo cell");
		for (int kind = 0; kind < 6; ++kind)
		{
			ComponentDefinition definition;
			definition.Kind = static_cast<EquipmentKind>(kind);
			definition.Name = std::string(ToString(definition.Kind));
			definition.Placement.Position = { (kind % 3 - 1) * 1.6, 0, (kind / 3 - .5) * 1.2 };
			definition.Properties.Stroke = .5;
			(void)machine.Create(definition);
		}
		return machine;
	}

	std::vector<unsigned char> Pixels(const std::filesystem::path& path)
	{
		int width = 0, height = 0, channels = 0;
		auto data = stbi_load(path.string().c_str(), &width, &height, &channels, 3);
		if (!data) throw std::runtime_error("Cannot read GPU capture");
		std::vector<unsigned char> result(data, data + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
		stbi_image_free(data);
		return result;
	}

	double Difference(const std::vector<unsigned char>& first, const std::vector<unsigned char>& second)
	{
		if (first.size() != second.size() || first.empty()) throw std::runtime_error("GPU capture dimensions differ");
		double sum = 0;
		for (std::size_t pixel = 0; pixel < first.size(); ++pixel)
			sum += std::abs(static_cast<int>(first[pixel]) - static_cast<int>(second[pixel]));
		return sum / static_cast<double>(first.size());
	}

	void Smoke(FactoryCore::SceneRenderer& renderer, FactoryCore::EditorSession& session, const std::filesystem::path& capture)
	{
		using namespace FactoryCore;
		renderer.RenderFrame(960, 600);
		renderer.Capture(capture);
		const auto baseline = Pixels(capture);
		const auto& position = session.GetMachine().GetDefinition(1).Placement.Position;
		const glm::vec4 point = renderer.GetProjectionMatrix() * renderer.GetViewMatrix()
			* glm::vec4(static_cast<float>(position.X), static_cast<float>(position.Y) + .35f, static_cast<float>(position.Z), 1.f);
		if (renderer.Pick((point.x / point.w + 1.f) * 480.f, (1.f - point.y / point.w) * 300.f) != 1)
			throw std::runtime_error("Viewport picking does not match the rendered transform");
		const auto [low, high] = std::minmax_element(baseline.begin(), baseline.end());
		if (*high - *low < 40) throw std::runtime_error("GPU output is blank or lacks scene contrast");
		auto variantPath = capture.parent_path() / (capture.stem().string() + "-Variant.png");
		auto checkFeature = [&](bool& feature, const char* name)
		{
			feature = false;
			renderer.RenderFrame(960, 600);
			renderer.Capture(variantPath);
			const double difference = Difference(baseline, Pixels(variantPath));
			feature = true;
			if (difference < 0.015) throw std::runtime_error(std::string(name) + " has no measurable effect on GPU output");
			std::cout << "PASS " << name << " mean pixel difference " << difference << '\n';
		};
		checkFeature(renderer.GetSettings().AmbientOcclusion, "SSAO");
		checkFeature(renderer.GetSettings().Shadows, "Soft shadows");
		checkFeature(renderer.GetSettings().ImageBasedLighting, "HDRI image-based lighting");
		// Verify visual model import and persistently edited glTF PBR material.
		const auto materials = renderer.GetMaterials(1);
		if (materials.empty()) throw std::runtime_error("Imported glTF has no PBR materials");
		auto material = materials.front();
		material.Color = glm::vec3(.1f, .7f, .15f);
		material.Metalness = .2f;
		material.Roughness = .7f;
		auto materialPath = capture.parent_path() / ("Material-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".gltf");
		const auto exported = renderer.ExportMaterial(1, material, materialPath);
		session.Edit([&](Machine& machine) { auto definition = machine.GetDefinition(1); definition.VisualModel = exported; machine.Configure(1, definition); });
		renderer.RenderFrame(960, 600);
		if (renderer.GetMaterials(1).front().Roughness < .69f) throw std::runtime_error("PBR material export/import did not round trip");
		session.Undo();
		renderer.RenderFrame(960, 600);
		std::filesystem::remove(materialPath);
		session.Play();
		session.SetInput({ 1, "Extend" }, true);
		for (int tick = 0; tick < 35; ++tick) session.Advance(.01);
		session.Pause();
		renderer.RenderFrame(960, 600);
		renderer.Capture(variantPath);
		if (Difference(baseline, Pixels(variantPath)) < .02) throw std::runtime_error("Simulated cylinder motion is not visible");
		session.Stop();
		renderer.RenderFrame(640, 480);
		renderer.RenderFrame(960, 600);
		renderer.Capture(capture);
		std::filesystem::remove(variantPath);
		std::cout << "PASS glTF materials/textures, PBR round trip, visible motion, resize and HDR capture\n";
	}
}

int main(int argc, char** argv)
{
	try
	{
		GraphicsMessages messages;
		donut::log::ConsoleApplicationMode();
		donut::log::SetCallback([&messages](donut::log::Severity severity, const char* message) { if (severity >= donut::log::Severity::Error) ++messages.Errors; std::cerr << message << std::endl; });
		LogResetGuard logGuard;
		bool smoke = false;
#ifdef FACTORYCORE_GPU_TESTS
		bool uiSmoke = false;
#endif
		bool validation = false;
		int frames = 0;
		std::filesystem::path capture = "Build/FactoryCore.png";
		std::filesystem::path machinePath;
		std::filesystem::path assets;
		for (int index = 1; index < argc; ++index)
		{
			const std::string argument = argv[index];
			auto value = [&]() -> std::string
			{
				if (++index == argc) throw std::invalid_argument("Missing value for " + argument);
				return argv[index];
			};
			if (argument == "--smoke") smoke = true;
#ifdef FACTORYCORE_GPU_TESTS
			else if (argument == "--ui-smoke") uiSmoke = true;
#endif
			else if (argument == "--capture") capture = value();
			else if (argument == "--machine") machinePath = value();
			else if (argument == "--assets") assets = value();
			else if (argument == "--validation") validation = true;
			else if (argument == "--frames")
			{
				const auto number = value();
				const auto parsed = std::from_chars(number.data(), number.data() + number.size(), frames);
				if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || frames < 1)
					throw std::invalid_argument("Frames must be a positive integer");
			}
			else if (argument == "--help")
			{
				std::cout << "FactoryCoreEditor [--machine file.factory] [--assets directory] [--validation]\n"
					<< "FactoryCoreEditor --smoke --capture output.png (headless Vulkan validation)\n";
				return 0;
			}
			else throw std::invalid_argument("Unknown editor option: " + argument);
		}
		if (smoke && (!machinePath.empty() || frames)) throw std::invalid_argument("The smoke test uses the built-in demo; omit --machine and --frames");
		if (assets.empty())
		{
			auto installed = std::filesystem::absolute(argv[0]).parent_path() / "../share/FactoryCore/Assets";
			assets = std::filesystem::is_directory(installed) ? installed : std::filesystem::path(FACTORYCORE_ASSET_DIRECTORY);
		}
#ifdef FACTORYCORE_GPU_TESTS
		if (uiSmoke)
		{
			if (capture.has_parent_path()) std::filesystem::create_directories(capture.parent_path());
			machinePath = capture.parent_path() / "UiMachine.factory";
			FactoryCore::SaveEditorMachine(Demo().GetConfiguration(), machinePath);
		}
#endif
		FactoryCore::EditorSession session(machinePath.empty() ? Demo() : FactoryCore::LoadEditorMachine(machinePath));
		std::unique_ptr<donut::app::DeviceManager> manager(donut::app::DeviceManager::Create(nvrhi::GraphicsAPI::VULKAN));
		donut::app::DeviceCreationParameters parameters;
		parameters.enableNvrhiValidationLayer = true;
		parameters.enableDebugRuntime = validation;
		parameters.enableWarningsAsErrors = validation;
		parameters.messageCallback = &messages;
		parameters.vsyncEnabled = true;
		parameters.backBufferWidth = 1600;
		parameters.backBufferHeight = 900;
		if (!(smoke ? manager->CreateHeadlessDevice(parameters) : manager->CreateWindowDeviceAndSwapChain(parameters, "FactoryCore")))
			throw std::runtime_error("Vulkan device creation failed; use a Vulkan 1.3 driver with required NVRHI features");
		{
			FactoryCore::SceneRenderer renderer(manager.get(), session, std::filesystem::absolute(assets));
			renderer.Initialize();
			if (!session.GetMachine().GetComponentIds().empty()) session.Select(session.GetMachine().GetComponentIds().front());
			if (smoke) Smoke(renderer, session, capture);
			else
			{
				FactoryCore::EditorUI ui(manager.get(), session, renderer, machinePath.string());
				if (!ui.Init(renderer.GetShaderFactory())) throw std::runtime_error("Cannot initialize editor UI");
				WindowCloseGuard closeGuard(manager->GetWindow(), ui);
				manager->AddRenderPassToBack(&renderer);
				manager->AddRenderPassToBack(&ui);
				if (frames) manager->m_callbacks.beforeFrame = [](donut::app::DeviceManager& deviceManager, uint32_t) { deviceManager.RenderNextFrameWhileUnfocused(); };
				if (frames)
					manager->m_callbacks.afterRender = [&](donut::app::DeviceManager&, uint32_t frame)
					{
						if (frame + 1 >= static_cast<uint32_t>(frames))
						{
							renderer.Capture(capture, manager->GetCurrentBackBuffer());
							glfwSetWindowShouldClose(manager->GetWindow(), GLFW_TRUE);
						}
					};
#ifdef FACTORYCORE_GPU_TESTS
				std::unique_ptr<FactoryCore::EditorGuiSmoke> guiTest;
				if (uiSmoke)
				{
					guiTest = std::make_unique<FactoryCore::EditorGuiSmoke>(ui, session, machinePath);
					manager->m_callbacks.beforeFrame = [&](donut::app::DeviceManager& deviceManager, uint32_t) { guiTest->BeforeFrame(deviceManager); };
					manager->m_callbacks.afterRender = [&](donut::app::DeviceManager&, uint32_t)
					{
						if (guiTest->IsComplete()) renderer.Capture(capture, manager->GetCurrentBackBuffer());
					};
				}
#endif
				manager->RunMessageLoop();
#ifdef FACTORYCORE_GPU_TESTS
				if (guiTest && !guiTest->IsComplete()) throw std::runtime_error("UI smoke test closed early");
#endif
				manager->RemoveRenderPass(&ui);
				manager->RemoveRenderPass(&renderer);
			}
			manager->GetDevice()->waitForIdle();
		}
		manager->Shutdown();
		const int graphicsErrors = messages.Errors.load();
		bool automatedRun = smoke || frames > 0 || validation;
#ifdef FACTORYCORE_GPU_TESTS
		automatedRun = automatedRun || uiSmoke;
#endif
		donut::log::ResetCallback();
		if (automatedRun && graphicsErrors != 0) throw std::runtime_error("NVRHI reported graphics validation errors");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FactoryCore editor: " << error.what() << '\n';
		return 1;
	}
}
