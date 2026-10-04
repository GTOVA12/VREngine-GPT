#pragma once
#include "FactoryCore/ProductCell.h"
#include <filesystem>

namespace FactoryCore
{
	std::vector<ComponentDefinition> GetProductCellVisualParts(const std::filesystem::path& assets);
}