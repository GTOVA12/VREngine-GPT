#pragma once
#include "FactoryCore/Machine.h"
#include <filesystem>

namespace FactoryCore
{
	// Visual paths in a saved machine are relative to that machine file.
	Machine LoadEditorMachine(const std::filesystem::path& path);
	void SaveEditorMachine(const MachineDefinition& definition, const std::filesystem::path& path);
}
