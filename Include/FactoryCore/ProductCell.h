#pragma once
#include "FactoryCore/Controller.h"
#include <array>

namespace FactoryCore
{
	enum class ProductCellState
	{
		Feeding,
		Locating,
		Stopping,
		Clamping,
		Extending,
		Holding,
		Retracting,
		Releasing,
		Discharging,
		OutfeedStopping,
		Complete,
		Faulted
	};

	struct ProductCellIO
	{
		bool Entry = false;
		bool Station = false;
		bool Exit = false;
		bool Clamped = false;
		bool Extended = false;
		bool Retracted = false;
		bool Processed = false;
		bool Drive = false;
		bool Clamp = false;
		bool Extend = false;
		bool Retract = false;
	};

	// Cyclic simulated PLC: reads an input image, executes a sequence, writes an output image.
	class ProductCellController final : public IController
	{
	  public:
		explicit ProductCellController(const Machine& machine);
		void Apply(Machine& machine) override;
		ProductCellState GetState() const
		{
			return m_State;
		}
		const ProductCellIO& GetIO() const
		{
			return m_IO;
		}
		std::uint64_t GetScans() const
		{
			return m_Scans;
		}
		unsigned GetCompletedProducts() const
		{
			return m_CompletedProducts;
		}
		const std::string& GetFaultReason() const
		{
			return m_FaultReason;
		}

	  private:
		void Scan(const Machine& machine);
		void Transition(ProductCellState state);
		std::array<ComponentId, 8> m_Parts{};
		ProductCellState m_State = ProductCellState::Feeding;
		ProductCellIO m_IO;
		double m_StateTime = 0.0;
		bool m_Marked = false;
		std::uint64_t m_Scans = 0;
		unsigned m_CompletedProducts = 0;
		std::string m_FaultReason;
	};

	// Idealized equipment specifications for the guided assembly and deterministic tests.
	std::vector<ComponentDefinition> GetProductCellParts();
	std::vector<Connection> GetProductCellConnections(const Machine& machine);
	std::string_view ToString(ProductCellState state);
}