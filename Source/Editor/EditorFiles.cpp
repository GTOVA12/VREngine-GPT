#include "FactoryCore/EditorFiles.h"
#include "FactoryCore/Persistence.h"

namespace FactoryCore
{
	Machine LoadEditorMachine(const std::filesystem::path& path)
	{
		auto definition = LoadConfigurationFile(path).GetConfiguration();
		const auto directory = std::filesystem::absolute(path).parent_path();
		for (auto& component : definition.Components)
			if (!component.VisualModel.empty())
				component.VisualModel = std::filesystem::absolute(directory / component.VisualModel).lexically_normal().generic_string();
		return Machine::FromConfiguration(definition);
	}

	void SaveEditorMachine(const MachineDefinition& definition, const std::filesystem::path& path)
	{
		auto relocatable = definition;
		const auto directory = std::filesystem::absolute(path).parent_path();
		for (auto& component : relocatable.Components)
			if (!component.VisualModel.empty())
			{
				const auto model = std::filesystem::absolute(component.VisualModel).lexically_normal();
				const auto relative = model.lexically_relative(directory.lexically_normal());
				// Different Windows drives cannot share a relative path.
				component.VisualModel = (relative.empty() ? model : relative).generic_string();
			}
		SaveConfigurationFile(Machine::FromConfiguration(relocatable), path);
	}
}
