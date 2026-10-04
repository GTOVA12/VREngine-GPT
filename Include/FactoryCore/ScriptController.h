#pragma once

#include "FactoryCore/Controller.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace FactoryCore
{
	class ScriptController final : public IController
	{
	public:
		explicit ScriptController(std::string_view source, int instructionBudget = 100000);
		~ScriptController() override;
		ScriptController(const ScriptController&) = delete;
		ScriptController& operator=(const ScriptController&) = delete;
		ScriptController(ScriptController&&) = delete;
		ScriptController& operator=(ScriptController&&) = delete;
		void RegisterPrefab(std::string name, MachineDefinition definition);
		void Apply(Machine& machine) override;
		bool HasFailed() const;

	private:
		struct Context;
		std::unique_ptr<Context> m_Context;
	};

	std::string ReadScriptFile(const std::string& path);
}
