#include "SceneRenderer.h"

#include <donut/core/vfs/VFS.h>
#include <donut/engine/CommonRenderPasses.h>
#include <donut/engine/FramebufferFactory.h>
#include <donut/engine/Scene.h>
#include <donut/engine/SceneGraph.h>
#include <donut/engine/ShaderFactory.h>
#include <donut/engine/TextureCache.h>
#include <donut/engine/View.h>
#include <donut/render/CascadedShadowMap.h>
#include <donut/render/DeferredLightingPass.h>
#include <donut/render/DepthPass.h>
#include <donut/render/DrawStrategy.h>
#include <donut/render/EnvironmentMapPass.h>
#include <donut/render/ForwardShadingPass.h>
#include <donut/render/GBuffer.h>
#include <donut/render/GBufferFillPass.h>
#include <donut/render/LightProbeProcessingPass.h>
#include <donut/render/SsaoPass.h>
#include <donut/render/ToneMappingPasses.h>
#include <json/json.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

namespace FactoryCore
{
	namespace
	{
		namespace Engine = donut::engine;
		namespace Render = donut::render;

		dm::double3 Convert(Vector3 value) { return { value.X, value.Y, value.Z }; }
		std::string NodeName(ComponentId id) { return "Equipment_" + std::to_string(id); }

		std::filesystem::path ModelPath(const ComponentDefinition& definition, const std::filesystem::path& assets)
		{
			return definition.VisualModel.empty()
				? assets / "Models" / (std::string(ToString(definition.Kind)) + ".gltf")
				: std::filesystem::absolute(definition.VisualModel);
		}

		class TemporaryScene
		{
		public:
			explicit TemporaryScene(const std::filesystem::path& parent = std::filesystem::temp_directory_path())
			{
				std::random_device random;
				for (int attempt = 0; attempt < 32; ++attempt)
				{
					m_Directory = std::filesystem::absolute(parent) / ("FactoryCore-" + std::to_string(random()));
					if (std::filesystem::create_directory(m_Directory)) return;
				}
				throw std::runtime_error("Cannot create a temporary scene directory");
			}
			~TemporaryScene()
			{
				std::error_code error;
				std::filesystem::remove_all(m_Directory, error);
			}
			std::filesystem::path GetPath(const char* name = "Scene.json") const { return m_Directory / name; }
		private:
			std::filesystem::path m_Directory;
		};

		nvrhi::TextureHandle Texture(nvrhi::IDevice* device, unsigned width, unsigned height,
			nvrhi::Format format, const char* name, bool uav = false)
		{
			nvrhi::TextureDesc desc;
			desc.width = width;
			desc.height = height;
			desc.format = format;
			desc.debugName = name;
			desc.isRenderTarget = true;
			desc.isUAV = uav;
			desc.initialState = nvrhi::ResourceStates::RenderTarget;
			desc.keepInitialState = true;
			return device->createTexture(desc);
		}
	}

	struct SceneRenderer::Implementation
	{
		nvrhi::DeviceHandle Device;
		EditorSession& Session;
		std::filesystem::path Assets;
		std::shared_ptr<donut::vfs::NativeFileSystem> FileSystem;
		std::shared_ptr<Engine::ShaderFactory> Shaders;
		std::shared_ptr<Engine::CommonRenderPasses> Common;
		std::shared_ptr<Engine::TextureCache> Textures;
		std::shared_ptr<Engine::Scene> Scene;
		std::shared_ptr<Engine::DirectionalLight> Sun;
		std::map<ComponentId, std::shared_ptr<Engine::SceneGraphNode>> Nodes;
		std::map<Engine::SceneGraphNode*, dm::double3> BaseTranslations;
		std::map<ComponentId, std::string> Topology;
		RenderSettings Settings;
		std::string Error;
		std::string RejectedTopology;
		nvrhi::CommandListHandle Commands;
		Engine::PlanarView View;
		std::unique_ptr<Render::GBufferRenderTargets> Targets;
		nvrhi::TextureHandle Hdr;
		nvrhi::TextureHandle Ldr;
		nvrhi::TextureHandle Occlusion;
		std::shared_ptr<Engine::FramebufferFactory> HdrFramebuffer;
		std::shared_ptr<Engine::FramebufferFactory> LdrFramebuffer;
		std::shared_ptr<Render::CascadedShadowMap> Shadow;
		std::shared_ptr<Engine::FramebufferFactory> ShadowFramebuffer;
		std::unique_ptr<Render::DepthPass> DepthPass;
		std::unique_ptr<Render::GBufferFillPass> GBufferPass;
		std::unique_ptr<Render::DeferredLightingPass> LightingPass;
		std::unique_ptr<Render::ForwardShadingPass> ForwardPass;
		std::unique_ptr<Render::SsaoPass> SsaoPass;
		std::unique_ptr<Render::ToneMappingPass> TonePass;
		std::unique_ptr<Render::EnvironmentMapPass> SkyPass;
		nvrhi::TextureHandle Environment;
		std::vector<std::shared_ptr<Engine::LightProbe>> Probes;
		glm::mat4 ViewMatrix{ 1.0f };
		glm::mat4 Projection{ 1.0f };
		glm::vec3 Target{ 0.0f, 0.35f, 0.0f };
		float Yaw = 0.65f;
		float Pitch = 0.45f;
		float Distance = 6.0f;
		unsigned Width = 0;
		unsigned Height = 0;
		uint32_t Frame = 0;

		Implementation(nvrhi::IDevice* device, EditorSession& session, std::filesystem::path assets)
			: Device(device), Session(session), Assets(std::move(assets)) {}

		void Camera()
		{
			const glm::vec3 offset{ std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch), std::cos(Pitch) * std::cos(Yaw) };
			ViewMatrix = glm::lookAtLH(Target + offset * Distance, Target, glm::vec3(0, 1, 0));
			Projection = glm::perspectiveLH_ZO(glm::radians(50.0f), static_cast<float>(Width) / static_cast<float>(Height), 0.05f, 200.0f);
			View.SetViewport(nvrhi::Viewport(static_cast<float>(Width), static_cast<float>(Height)));
			// GLM column-major storage is the transposed row-major matrix used by Donut.
			View.SetMatrices(dm::homogeneousToAffine(dm::float4x4(glm::value_ptr(ViewMatrix))),
				dm::float4x4(glm::value_ptr(Projection)));
			View.UpdateCache();
		}

		void Resize(unsigned width, unsigned height)
		{
			if (width == Width && height == Height && Targets) return;
			if (width == 0 || height == 0 || width > 8192 || height > 8192)
				throw std::invalid_argument("Viewport dimensions must be in [1, 8192]");
			Device->waitForIdle();
			Width = width;
			Height = height;
			Camera();
			Targets = std::make_unique<Render::GBufferRenderTargets>();
			Targets->Init(Device, dm::uint2(width, height), 1, false, false);
			Hdr = Texture(Device, width, height, nvrhi::Format::RGBA16_FLOAT, "FactoryCore HDR", true);
			Ldr = Texture(Device, width, height, nvrhi::Format::SRGBA8_UNORM, "FactoryCore display");
			Occlusion = Texture(Device, width, height, nvrhi::Format::R8_UNORM, "FactoryCore SSAO", true);
			HdrFramebuffer = std::make_shared<Engine::FramebufferFactory>(Device);
			HdrFramebuffer->RenderTargets = { Hdr };
			HdrFramebuffer->DepthTarget = Targets->Depth;
			LdrFramebuffer = std::make_shared<Engine::FramebufferFactory>(Device);
			LdrFramebuffer->RenderTargets = { Ldr };
			SsaoPass = std::make_unique<Render::SsaoPass>(Device, Shaders, Common, Targets->Depth, Targets->GBufferNormals, Occlusion);
			TonePass = std::make_unique<Render::ToneMappingPass>(Device, Shaders, Common, LdrFramebuffer, View, Render::ToneMappingPass::CreateParameters{});
			Commands->open();
			TonePass->ResetExposure(Commands, 0.5f);
			Commands->close();
			Device->executeCommandList(Commands);
			LightingPass->ResetBindingCache();
			SkyPass = Environment ? std::make_unique<Render::EnvironmentMapPass>(Device, Shaders, Common, HdrFramebuffer, View, Environment) : nullptr;
		}

		void LoadScene()
		{
			std::map<ComponentId, std::string> topology;
			for (auto id : Session.GetMachine().GetComponentIds())
				topology[id] = ModelPath(Session.GetMachine().GetDefinition(id), Assets).generic_string();
			if (Scene && topology == Topology) return;
			std::string signature;
			for (const auto& [id, path] : topology) signature += std::to_string(id) + path + "\n";
			if (!RejectedTopology.empty() && signature == RejectedTopology) return;
			try
			{
				Json::Value document(Json::objectValue);
				document["models"] = Json::Value(Json::arrayValue);
				document["graph"] = Json::Value(Json::arrayValue);
				for (const auto& [id, path] : topology)
				{
					if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Model does not exist: " + path);
					Json::Value node;
					node["name"] = NodeName(id);
					node["model"] = document["models"].size();
					document["models"].append(path);
					document["graph"].append(node);
				}
				Json::Value floor;
				floor["name"] = "Floor";
				floor["model"] = document["models"].size();
				document["models"].append((Assets / "Models/Floor.gltf").generic_string());
				document["graph"].append(floor);
				TemporaryScene temporary;
				{
					std::ofstream output(temporary.GetPath());
					output << document;
					if (!output) throw std::runtime_error("Cannot write temporary render scene");
				}
				auto textureCache = std::make_shared<Engine::TextureCache>(Device, FileSystem, nullptr);
				textureCache->SetMaxTextureSize(4096);
				auto replacement = std::make_shared<Engine::Scene>(Device, *Shaders, FileSystem, textureCache, nullptr, nullptr);
				if (!replacement->Load(temporary.GetPath())) throw std::runtime_error("glTF scene import failed");
				textureCache->ProcessRenderingThreadCommands(*Common, 0.f);
				textureCache->LoadingFinished();
				replacement->FinishedLoading(Frame);
				auto graph = replacement->GetSceneGraph();
				std::map<ComponentId, std::shared_ptr<Engine::SceneGraphNode>> nodes;
				std::map<Engine::SceneGraphNode*, dm::double3> translations;
				for (const auto& [id, path] : topology)
				{
					(void)path;
					auto node = graph->FindNode(NodeName(id), graph->GetRootNode().get());
					if (!node || node->GetNumChildren() == 0) throw std::runtime_error("Imported model has no scene geometry");
					nodes[id] = node;
					for (Engine::SceneGraphWalker walker(node.get()); walker; walker.Next(true))
						translations[walker.Get()] = walker->GetTranslation();
				}
				auto lightNode = std::make_shared<Engine::SceneGraphNode>();
				lightNode->SetName("FactoryCore Sun");
				auto sun = std::make_shared<Engine::DirectionalLight>();
				sun->irradiance = 2.0f;
				sun->angularSize = Settings.SunAngle;
				lightNode->SetLeaf(sun);
				graph->Attach(graph->GetRootNode(), lightNode);
				sun->SetDirection(dm::double3(-0.6, -1.0, -0.3));
				sun->shadowMap = Shadow;
				Device->waitForIdle();
				GBufferPass->ResetBindingCache();
				DepthPass->ResetBindingCache();
				ForwardPass->ResetBindingCache();
				LightingPass->ResetBindingCache();
				Textures = std::move(textureCache);
				Scene = std::move(replacement);
				Nodes = std::move(nodes);
				BaseTranslations = std::move(translations);
				Sun = std::move(sun);
				Topology = std::move(topology);
				Error.clear();
				RejectedTopology.clear();
			}
			catch (const std::exception& error)
			{
				Error = error.what();
				RejectedTopology = signature;
				if (!Scene) throw;
			}
		}

		void UpdateEquipment()
		{
			const auto& machine = Session.GetMachine();
			const auto ids = machine.GetComponentIds();
			for (const auto& [id, node] : Nodes)
			{
				if (std::find(ids.begin(), ids.end(), id) == ids.end()) continue;
				const auto& definition = machine.GetDefinition(id);
				const auto& state = machine.GetState(id);
				node->SetTranslation(Convert(definition.Placement.Position));
				node->SetScaling(Convert(definition.Placement.Scale));
				const auto& euler = definition.Placement.Rotation;
				const glm::dquat rotation(glm::dvec3(euler.X, euler.Y, euler.Z));
				node->SetRotation(dm::dquat(rotation.w, rotation.x, rotation.y, rotation.z));
				for (Engine::SceneGraphWalker walker(node.get()); walker; walker.Next(true))
				{
					auto child = walker.Get();
					auto translation = BaseTranslations.at(child);
					if (child->GetName() == "Motion")
					{
						switch (definition.Kind)
						{
						case EquipmentKind::Cylinder:
						case EquipmentKind::Workpiece: translation.x += state.Position; break;
						case EquipmentKind::Conveyor: translation.x += std::remainder(state.Position, 1.5); break;
						case EquipmentKind::Actuator: translation.y += state.Active ? 0.1 : 0.0; break;
						case EquipmentKind::Motor:
						{
							const glm::dquat spin = glm::angleAxis(state.Position, glm::dvec3(1, 0, 0));
							child->SetRotation(dm::dquat(spin.w, spin.x, spin.y, spin.z));
							break;
						}
						case EquipmentKind::Sensor: break;
						}
						child->SetTranslation(translation);
					}
					if (child->GetName() == "Fault")
						child->SetScaling(state.Fault == FaultCode::None ? dm::double3(0.00001) : dm::double3(0.06, 0.04, 0.06));
					if (child->GetName() == "Indicator")
						child->SetScaling(state.Active ? dm::double3(.045, .015, .045) : dm::double3(.015, .005, .015));
				}
			}
			Sun->angularSize = Settings.SunAngle;
			Sun->shadowMap = Settings.Shadows ? Shadow : nullptr;
		}
	};

	SceneRenderer::SceneRenderer(donut::app::DeviceManager* manager, EditorSession& session, std::filesystem::path assets)
		: IRenderPass(manager), m_Impl(std::make_unique<Implementation>(manager->GetDevice(), session, std::move(assets))) {}
	SceneRenderer::~SceneRenderer() = default;

	void SceneRenderer::Initialize()
	{
		auto& impl = *m_Impl;
		impl.FileSystem = std::make_shared<donut::vfs::NativeFileSystem>();
		impl.Shaders = std::make_shared<Engine::ShaderFactory>(impl.Device, impl.FileSystem, "");
		impl.Common = std::make_shared<Engine::CommonRenderPasses>(impl.Device, impl.Shaders);
		impl.Textures = std::make_shared<Engine::TextureCache>(impl.Device, impl.FileSystem, nullptr);
		impl.Textures->SetMaxTextureSize(4096);
		impl.Commands = impl.Device->createCommandList();
		impl.Shadow = std::make_shared<Render::CascadedShadowMap>(impl.Device, 2048, 4, 0, nvrhi::Format::D32);
		impl.Shadow->SetupProxyViews();
		impl.ShadowFramebuffer = std::make_shared<Engine::FramebufferFactory>(impl.Device);
		impl.ShadowFramebuffer->DepthTarget = impl.Shadow->GetTexture();
		impl.DepthPass = std::make_unique<Render::DepthPass>(impl.Device, impl.Common);
		Render::DepthPass::CreateParameters depth;
		depth.depthBias = 100;
		depth.slopeScaledDepthBias = 4.0f;
		impl.DepthPass->Init(*impl.Shaders, depth);
		impl.GBufferPass = std::make_unique<Render::GBufferFillPass>(impl.Device, impl.Common);
		impl.GBufferPass->Init(*impl.Shaders, {});
		impl.LightingPass = std::make_unique<Render::DeferredLightingPass>(impl.Device, impl.Common);
		impl.LightingPass->Init(impl.Shaders);
		impl.ForwardPass = std::make_unique<Render::ForwardShadingPass>(impl.Device, impl.Common);
		impl.ForwardPass->Init(*impl.Shaders, {});
		impl.Resize(1280, 720);
		impl.LoadScene();
		SetEnvironment(impl.Assets / "Environments/StudioSmall09.hdr");
	}

	void SceneRenderer::SetEnvironment(const std::filesystem::path& path)
	{
		auto& impl = *m_Impl;
		impl.Device->waitForIdle();
		impl.Commands->open();
		auto loaded = impl.Textures->LoadTextureFromFile(path, Engine::TextureLoadOptions{ Engine::SRGBMode::ForceLinear }, impl.Common.get(), impl.Commands);
		if (!loaded || !loaded->texture)
		{
			impl.Commands->close();
			impl.Device->executeCommandList(impl.Commands);
			throw std::runtime_error("Cannot load HDR environment: " + path.string());
		}
		nvrhi::TextureDesc cubeDesc;
		cubeDesc.width = cubeDesc.height = 256;
		cubeDesc.arraySize = 6;
		cubeDesc.mipLevels = 9;
		cubeDesc.dimension = nvrhi::TextureDimension::TextureCube;
		cubeDesc.format = nvrhi::Format::RGBA16_FLOAT;
		cubeDesc.isRenderTarget = true;
		cubeDesc.initialState = nvrhi::ResourceStates::ShaderResource;
		cubeDesc.keepInitialState = true;
		cubeDesc.debugName = "FactoryCore environment cube";
		auto cube = impl.Device->createTexture(cubeDesc);
		auto cubeFramebuffer = std::make_shared<Engine::FramebufferFactory>(impl.Device);
		cubeFramebuffer->RenderTargets = { cube };
		auto depthDesc = cubeDesc;
		depthDesc.dimension = nvrhi::TextureDimension::Texture2DArray;
		depthDesc.mipLevels = 1;
		depthDesc.format = nvrhi::Format::D32;
		depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
		depthDesc.debugName = "FactoryCore environment depth";
		auto cubeDepth = impl.Device->createTexture(depthDesc);
		cubeFramebuffer->DepthTarget = cubeDepth;
		impl.Commands->clearDepthStencilTexture(cubeDepth, nvrhi::AllSubresources, true, 1.0f, false, 0);
		Engine::CubemapView cubeView;
		cubeView.SetTransform(dm::affine3::identity(), 0.1f, 100.0f, false);
		cubeView.SetArrayViewports(256, 0);
		cubeView.UpdateCache();
		Render::EnvironmentMapPass convert(impl.Device, impl.Shaders, impl.Common, cubeFramebuffer, cubeView, loaded->texture);
		convert.Render(impl.Commands, cubeView);
		Render::LightProbeProcessingPass processing(impl.Device, impl.Shaders, impl.Common, 256);
		processing.GenerateCubemapMips(impl.Commands, cube, 0, 0, 8);
		cubeDesc.dimension = nvrhi::TextureDimension::TextureCubeArray;
		cubeDesc.width = cubeDesc.height = 32;
		cubeDesc.mipLevels = 1;
		cubeDesc.debugName = "FactoryCore diffuse irradiance";
		auto diffuse = impl.Device->createTexture(cubeDesc);
		cubeDesc.width = cubeDesc.height = 128;
		cubeDesc.mipLevels = 8;
		cubeDesc.debugName = "FactoryCore specular IBL";
		auto specular = impl.Device->createTexture(cubeDesc);
		processing.RenderDiffuseMap(impl.Commands, cube, nvrhi::AllSubresources, diffuse, 0, 0);
		for (unsigned mip = 0; mip < 8; ++mip)
			processing.RenderSpecularMap(impl.Commands, static_cast<float>(mip) / 7.0f, cube, nvrhi::AllSubresources, specular, 0, mip);
		processing.RenderEnvironmentBrdfTexture(impl.Commands);
		auto probe = std::make_shared<Engine::LightProbe>();
		probe->diffuseMap = diffuse;
		probe->specularMap = specular;
		probe->environmentBrdf = processing.GetEnvironmentBrdfTexture();
		impl.Commands->close();
		impl.Device->executeCommandList(impl.Commands);
		impl.Device->waitForIdle();
		impl.Probes = { probe };
		impl.Environment = loaded->texture;
		impl.SkyPass = std::make_unique<Render::EnvironmentMapPass>(impl.Device, impl.Shaders, impl.Common, impl.HdrFramebuffer, impl.View, impl.Environment);
	}

	void SceneRenderer::Animate(float elapsedSeconds)
	{
		try { m_Impl->Session.Advance(std::min(static_cast<double>(elapsedSeconds), 1.0)); }
		catch (const std::exception& error) { m_Impl->Error = error.what(); }
	}

	void SceneRenderer::BackBufferResizing()
	{
		m_Impl->Device->waitForIdle();
		m_Impl->Width = 0;
	}

	void SceneRenderer::Render(nvrhi::IFramebuffer* framebuffer)
	{
		const auto& info = framebuffer->getFramebufferInfo();
		RenderFrame(info.width, info.height, framebuffer);
	}

	void SceneRenderer::RenderFrame(unsigned width, unsigned height, nvrhi::IFramebuffer* output)
	{
		auto& impl = *m_Impl;
		impl.Resize(width, height);
		impl.Camera();
		impl.LoadScene();
		impl.UpdateEquipment();
		impl.Scene->RefreshSceneGraph(impl.Frame);
		impl.Commands->open();
		impl.Scene->RefreshBuffers(impl.Commands, impl.Frame);
		auto root = impl.Scene->GetSceneGraph()->GetRootNode();
		Render::InstancedOpaqueDrawStrategy opaque;
		if (impl.Settings.Shadows)
		{
			impl.Shadow->SetupForPlanarViewStable(*impl.Sun, impl.View.GetProjectionFrustum(), impl.View.GetInverseViewMatrix(), 35.0f, 40.0f, 40.0f);
			impl.Shadow->Clear(impl.Commands);
			Render::DepthPass::Context context;
			Render::RenderCompositeView(impl.Commands, &impl.Shadow->GetView(), nullptr, *impl.ShadowFramebuffer, root, opaque, *impl.DepthPass, context, "Soft shadows");
		}
		impl.Targets->Clear(impl.Commands);
		impl.Commands->clearTextureFloat(impl.Hdr, nvrhi::AllSubresources, nvrhi::Color(0.0f));
		Render::GBufferFillPass::Context gbuffer;
		Render::RenderCompositeView(impl.Commands, &impl.View, &impl.View, *impl.Targets->GBufferFramebuffer, root, opaque, *impl.GBufferPass, gbuffer, "GBuffer");
		if (impl.Settings.AmbientOcclusion)
		{
			Render::SsaoParameters params;
			params.radiusWorld = impl.Settings.OcclusionRadius;
			params.surfaceBias = 0.02f;
			impl.SsaoPass->Render(impl.Commands, params, impl.View);
		}
		Render::DeferredLightingPass::Inputs lighting;
		lighting.SetGBuffer(*impl.Targets);
		lighting.output = impl.Hdr;
		lighting.lights = &impl.Scene->GetSceneGraph()->GetLights();
		lighting.lightProbes = impl.Settings.ImageBasedLighting ? &impl.Probes : nullptr;
		lighting.ambientOcclusion = impl.Settings.AmbientOcclusion ? impl.Occlusion.Get() : nullptr;
		impl.LightingPass->Render(impl.Commands, impl.View, lighting);
		if (impl.SkyPass) impl.SkyPass->Render(impl.Commands, impl.View);
		Render::TransparentDrawStrategy transparent;
		Render::ForwardShadingPass::Context forward;
		impl.ForwardPass->PrepareLights(forward, impl.Commands, impl.Scene->GetSceneGraph()->GetLights(), 0.f, 0.f, impl.Settings.ImageBasedLighting ? impl.Probes : std::vector<std::shared_ptr<Engine::LightProbe>>{});
		Render::RenderCompositeView(impl.Commands, &impl.View, nullptr, *impl.HdrFramebuffer, root, transparent, *impl.ForwardPass, forward, "Transparent glTF");
		Render::ToneMappingParameters tone;
		tone.exposureBias = impl.Settings.Exposure;
		// Fixed luminance makes captures reproducible and preserves the exposure control.
		tone.minAdaptedLuminance = tone.maxAdaptedLuminance = 0.5f;
		impl.TonePass->AdvanceFrame(1.0f / 60.0f);
		impl.TonePass->SimpleRender(impl.Commands, tone, impl.View, impl.Hdr);
		if (output) impl.Common->BlitTexture(impl.Commands, output, impl.Ldr);
		impl.Commands->close();
		impl.Device->executeCommandList(impl.Commands);
		++impl.Frame;
		impl.Device->runGarbageCollection();
	}

	void SceneRenderer::Capture(const std::filesystem::path& path, nvrhi::ITexture* texture)
	{
		m_Impl->Device->waitForIdle();
		if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
		if (!Engine::SaveTextureToFile(m_Impl->Device, m_Impl->Common.get(), texture ? texture : m_Impl->Ldr.Get(), texture ? texture->getDesc().initialState : nvrhi::ResourceStates::RenderTarget, path.string().c_str()))
			throw std::runtime_error("Cannot save renderer capture");
	}

	void SceneRenderer::Orbit(float dx, float dy)
	{
		m_Impl->Yaw += dx * 0.005f;
		m_Impl->Pitch = std::clamp(m_Impl->Pitch + dy * 0.005f, -1.4f, 1.4f);
	}
	void SceneRenderer::Pan(float dx, float dy)
	{
		const auto inverse = glm::inverse(m_Impl->ViewMatrix);
		m_Impl->Target += (glm::vec3(inverse[0]) * -dx + glm::vec3(inverse[1]) * dy) * m_Impl->Distance * 0.001f;
	}
	void SceneRenderer::Zoom(float delta) { m_Impl->Distance = std::clamp(m_Impl->Distance * std::exp(-delta * 0.12f), 0.2f, 150.0f); }
	void SceneRenderer::Focus(ComponentId id)
	{
		const auto position = m_Impl->Session.GetMachine().GetDefinition(id).Placement.Position;
		m_Impl->Target = glm::vec3(static_cast<float>(position.X), static_cast<float>(position.Y) + .35f, static_cast<float>(position.Z));
		m_Impl->Distance = 3.0f;
	}
	ComponentId SceneRenderer::Pick(float x, float y) const
	{
		const auto& impl = *m_Impl;
		const auto inverse = glm::inverse(impl.Projection * impl.ViewMatrix);
		const glm::vec2 clip(x * 2.0f / static_cast<float>(impl.Width) - 1.0f, 1.0f - y * 2.0f / static_cast<float>(impl.Height));
		auto start = inverse * glm::vec4(clip, 0.0f, 1.0f);
		auto end = inverse * glm::vec4(clip, 1.0f, 1.0f);
		start /= start.w; end /= end.w;
		const glm::vec3 direction = glm::normalize(glm::vec3(end - start));
		float nearest = std::numeric_limits<float>::max();
		ComponentId selected = 0;
		for (const auto& [id, node] : impl.Nodes)
		{
			const auto bounds = node->GetGlobalBoundingBox();
			if (bounds.isempty()) continue;
			float low = 0.0f, high = nearest;
			for (int axis = 0; axis < 3 && low <= high; ++axis)
			{
				if (std::abs(direction[axis]) < 1.e-7f)
				{
					if (start[axis] < bounds.m_mins[axis] || start[axis] > bounds.m_maxs[axis]) high = -1.0f;
				}
				else
				{
					float a = (bounds.m_mins[axis] - start[axis]) / direction[axis];
					float b = (bounds.m_maxs[axis] - start[axis]) / direction[axis];
					if (a > b) std::swap(a, b);
					low = std::max(low, a); high = std::min(high, b);
				}
			}
			if (low <= high && low < nearest) { nearest = low; selected = id; }
		}
		return selected;
	}

	const glm::mat4& SceneRenderer::GetViewMatrix() const { return m_Impl->ViewMatrix; }
	const glm::mat4& SceneRenderer::GetProjectionMatrix() const { return m_Impl->Projection; }
	std::shared_ptr<Engine::ShaderFactory> SceneRenderer::GetShaderFactory() const { return m_Impl->Shaders; }
	const std::filesystem::path& SceneRenderer::GetAssetsDirectory() const { return m_Impl->Assets; }
	RenderSettings& SceneRenderer::GetSettings() { return m_Impl->Settings; }
	const std::string& SceneRenderer::GetLastError() const { return m_Impl->Error; }

	void SceneRenderer::CheckModel(const std::filesystem::path& path)
	{
		if (path.extension() != ".gltf" && path.extension() != ".glb")
			throw std::invalid_argument("Visual models must be glTF (.gltf or .glb)");
		auto textures = std::make_shared<Engine::TextureCache>(m_Impl->Device, m_Impl->FileSystem, nullptr);
		auto candidate = std::make_shared<Engine::Scene>(m_Impl->Device, *m_Impl->Shaders, m_Impl->FileSystem, textures, nullptr, nullptr);
		if (!candidate->Load(path) || candidate->GetSceneGraph()->GetMeshInstances().empty())
			throw std::runtime_error("Model import failed or contains no mesh instances");
		m_Impl->Topology.clear();
		m_Impl->RejectedTopology.clear();
	}

	std::vector<MaterialInfo> SceneRenderer::GetMaterials(ComponentId id) const
	{
		auto found = m_Impl->Nodes.find(id);
		if (found == m_Impl->Nodes.end()) return {};
		std::set<int> indices;
		std::vector<MaterialInfo> result;
		for (Engine::SceneGraphWalker walker(found->second.get()); walker; walker.Next(true))
		{
			auto instance = std::dynamic_pointer_cast<Engine::MeshInstance>(walker->GetLeaf());
			if (!instance) continue;
			for (const auto& geometry : instance->GetMesh()->geometries)
			{
				const auto& material = geometry->material;
				if (!indices.insert(material->materialIndexInModel).second) continue;
				result.push_back({ material->name, material->materialIndexInModel,
					{ material->baseOrDiffuseColor.x, material->baseOrDiffuseColor.y, material->baseOrDiffuseColor.z },
					material->metalness, material->roughness });
			}
		}
		return result;
	}

	std::string SceneRenderer::ExportMaterial(ComponentId id, const MaterialInfo& material, const std::filesystem::path& destination)
	{
		for (int channel = 0; channel < 3; ++channel)
			if (!std::isfinite(material.Color[channel]) || material.Color[channel] < 0.f || material.Color[channel] > 1.f)
				throw std::invalid_argument("Material base color must be finite and in [0, 1]");
		if (!std::isfinite(material.Metalness) || !std::isfinite(material.Roughness) || material.Metalness < 0.f || material.Metalness > 1.f || material.Roughness < 0.f || material.Roughness > 1.f)
			throw std::invalid_argument("Material metalness and roughness must be finite and in [0, 1]");
		const auto source = ModelPath(m_Impl->Session.GetMachine().GetDefinition(id), m_Impl->Assets);
		if (source.extension() != ".gltf" || destination.extension() != ".gltf")
			throw std::invalid_argument("Material export requires a .gltf source and destination; convert binary glTF before editing");
		if (std::filesystem::equivalent(source, std::filesystem::exists(destination) ? destination : source.parent_path()))
			throw std::invalid_argument("Export to a new glTF file to preserve the source asset");
		Json::Value document;
		std::ifstream input(source);
		Json::CharReaderBuilder reader;
		std::string error;
		if (!Json::parseFromStream(reader, input, &document, &error)) throw std::runtime_error("Invalid glTF JSON: " + error);
		if (material.Index < 0 || static_cast<Json::ArrayIndex>(material.Index) >= document["materials"].size())
			throw std::invalid_argument("Material index is outside this glTF asset");
		auto absolute = std::filesystem::absolute(destination);
		for (const char* collection : { "buffers", "images" })
			for (auto& entry : document[collection])
				if (entry.isMember("uri") && entry["uri"].asString().rfind("data:", 0) != 0)
				{
					const auto resource = std::filesystem::absolute(source.parent_path() / entry["uri"].asString()).lexically_normal();
					const auto relative = resource.lexically_relative(absolute.parent_path().lexically_normal());
					if (relative.empty()) throw std::invalid_argument("Export material on the same drive as its source resources");
					entry["uri"] = relative.generic_string();
				}
		auto& pbr = document["materials"][material.Index]["pbrMetallicRoughness"];
		const float opacity = pbr["baseColorFactor"].size() == 4 ? pbr["baseColorFactor"][3].asFloat() : 1.f;
		pbr["baseColorFactor"] = Json::Value(Json::arrayValue);
		for (int channel = 0; channel < 3; ++channel) pbr["baseColorFactor"].append(material.Color[channel]);
		pbr["baseColorFactor"].append(opacity);
		pbr["metallicFactor"] = material.Metalness;
		pbr["roughnessFactor"] = material.Roughness;
		if (std::filesystem::exists(absolute)) throw std::invalid_argument("Material destination already exists; choose a new file");
		std::filesystem::create_directories(absolute.parent_path());
		TemporaryScene temporary(absolute.parent_path());
		std::ofstream output(temporary.GetPath("Material.gltf"));
		output << document;
		output.close();
		if (!output) throw std::runtime_error("Cannot write edited glTF asset");
		if (!MoveFileExW(temporary.GetPath("Material.gltf").c_str(), absolute.c_str(), MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Cannot publish edited glTF asset; choose an unused writable destination");
		try { CheckModel(absolute); }
		catch (...) { std::filesystem::remove(absolute); throw; }
		return absolute.generic_string();
	}
}
