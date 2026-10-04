#pragma once

#include "FactoryCore/EditorSession.h"

#include <donut/app/DeviceManager.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace donut::engine { class ShaderFactory; }
namespace FactoryCore
{
	struct MaterialInfo
	{
		std::string Name;
		int Index = 0;
		glm::vec3 Color{ 1.0f };
		float Metalness = 0.0f;
		float Roughness = 0.5f;
	};

	struct RenderSettings
	{
		bool AmbientOcclusion = true;
		bool Shadows = true;
		bool ImageBasedLighting = true;
		float Exposure = 0.0f;
		float OcclusionRadius = 0.3f;
		float SunAngle = 1.5f;
	};

	class SceneRenderer final : public donut::app::IRenderPass
	{
	public:
		SceneRenderer(donut::app::DeviceManager* manager, EditorSession& session, std::filesystem::path assets);
		~SceneRenderer() override;
		void Initialize();
		void Animate(float elapsedSeconds) override;
		void Render(nvrhi::IFramebuffer* framebuffer) override;
		void BackBufferResizing() override;
		void RenderFrame(unsigned width, unsigned height, nvrhi::IFramebuffer* output = nullptr);
		void Capture(const std::filesystem::path& path, nvrhi::ITexture* texture = nullptr);
		void SetEnvironment(const std::filesystem::path& path);
		void Orbit(float dx, float dy);
		void Pan(float dx, float dy);
		void Zoom(float delta);
		void Focus(ComponentId id);
		ComponentId Pick(float x, float y) const;
		const glm::mat4& GetViewMatrix() const;
		const glm::mat4& GetProjectionMatrix() const;
		std::vector<MaterialInfo> GetMaterials(ComponentId id) const;
		std::string ExportMaterial(ComponentId id, const MaterialInfo& material, const std::filesystem::path& destination);
		void CheckModel(const std::filesystem::path& path);
		std::shared_ptr<donut::engine::ShaderFactory> GetShaderFactory() const;
		const std::filesystem::path& GetAssetsDirectory() const;
		RenderSettings& GetSettings();
		const std::string& GetLastError() const;

	private:
		struct Implementation;
		std::unique_ptr<Implementation> m_Impl;
	};
}
