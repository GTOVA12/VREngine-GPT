#include "FactoryCore/ProductCellAssembly.h"

namespace FactoryCore
{
	std::vector<ComponentDefinition> GetProductCellVisualParts(const std::filesystem::path& assets)
	{
		auto parts = GetProductCellParts();
		for (auto& part : parts)
		{
			const char* model = part.Kind == EquipmentKind::Conveyor	? "CellConveyor"
								: part.Kind == EquipmentKind::Motor		? "CellMotor"
								: part.Kind == EquipmentKind::Workpiece ? "CellProduct"
								: part.Kind == EquipmentKind::Sensor	? "CellSensor"
								: part.Kind == EquipmentKind::Cylinder	? "CellPress"
																		: "CellClamp";
			part.VisualModel = std::filesystem::absolute(assets / "Models" / (std::string(model) + ".gltf")).generic_string();
		}
		return parts;
	}
}