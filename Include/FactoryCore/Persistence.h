#pragma once

#include "FactoryCore/Machine.h"

#include <filesystem>
#include <iosfwd>

namespace FactoryCore
{
	// Version 1 configuration format. Live inputs, faults and ticks are not persisted.
	void SaveConfiguration(const Machine& machine, std::ostream& output);
	Machine LoadConfiguration(std::istream& input);
	void SaveConfigurationFile(const Machine& machine, const std::filesystem::path& path);
	Machine LoadConfigurationFile(const std::filesystem::path& path);
}
