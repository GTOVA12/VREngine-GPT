#include "FactoryCore/EditorSession.h"
#include "FactoryCore/EditorFiles.h"
#include "FactoryCore/Persistence.h"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
	void Check(bool condition) { if (!condition) throw std::runtime_error("Editor session assertion failed"); }
	template<typename Function> void Throws(Function&& function)
	{
		bool threw = false;
		try { function(); } catch (const std::exception&) { threw = true; }
		Check(threw);
	}
}

int main()
{
	try
	{
		using namespace FactoryCore;
		EditorSession session;
		ComponentDefinition definition;
		definition.Name = "Cylinder";
		ComponentId id = 0;
		session.Edit([&](Machine& machine) { id = machine.Create(definition); });
		session.Select(id);
		Check(session.CanUndo() && session.IsModified());
		session.Undo();
		Check(session.GetMachine().GetComponentIds().empty() && session.GetSelection() == 0 && !session.IsModified());
		session.Redo();
		Check(session.GetMachine().GetComponentIds().size() == 1);
		session.Select(id);
		session.MarkSaved();
		Check(!session.IsModified());
		const auto revision = session.GetRevision();
		Throws([&] { session.Edit([&](Machine& machine) { machine.Remove(id); machine.Remove(id); }); });
		Check(session.GetMachine().GetComponentIds().size() == 1 && session.GetRevision() == revision);

		session.BeginTransaction();
		Throws([&] { session.Play(); });
		Throws([&] { session.StepOnce(); });
		Check(session.IsEditing() && session.HasTransaction() && session.GetMachine().GetTick() == 0);
		for (int position = 1; position <= 10; ++position)
			session.Edit([&](Machine& machine) { auto placement = machine.GetDefinition(id).Placement; placement.Position.X = position; machine.Place(id, placement); });
		session.CommitTransaction();
		session.Undo();
		Check(session.GetMachine().GetDefinition(id).Placement.Position.X == 0);
		session.Redo();
		Check(session.GetMachine().GetDefinition(id).Placement.Position.X == 10);
		session.BeginTransaction();
		session.Edit([&](Machine& machine) { machine.Remove(id); });
		session.CancelTransaction();
		Check(session.GetMachine().GetComponentIds().size() == 1);
		auto controller = std::make_unique<CylinderCycleController>(id, 2);
		controller->Start();
		session.Play(std::move(controller));
		session.Advance(0.05);
		Check(session.GetCycleState() == CycleState::Extending);
		Check(session.GetMachine().GetTick() == 5);
		Check(std::abs(session.GetMachine().GetState(id).Position - 0.05) < 1.0e-10);
		Throws([&] { session.Edit([&](Machine& machine) { machine.Remove(id); }); });
		Check(!session.CanUndo());
		session.Pause();
		session.StepOnce();
		Check(session.GetMachine().GetTick() == 6);
		session.Play();
		session.Advance(0.04);
		Check(session.GetMachine().GetTick() == 10);
		session.Stop();
		Check(session.GetMachine().GetTick() == 0 && session.IsEditing());
		Check(session.GetMachine().GetState(id).Position == 0);
		Check(session.GetMachine().GetDefinition(id).Placement.Position.X == 10);
		Throws([&] { session.Advance(-1); });
		Throws([&] { session.Advance(std::numeric_limits<double>::infinity()); });
		EditorSession tiny(Machine("Tiny timestep", 1.0e-6));
		tiny.Play();
		tiny.Advance(0.1);
		Check(!tiny.IsPlaying() && !tiny.GetLastError().empty());
		Check(tiny.GetMachine().GetTick() == 256);
		tiny.Stop();
		Check(tiny.GetLastError().empty());
		const auto directory = std::filesystem::current_path() / "Testing/EditorFiles";
		std::filesystem::create_directories(directory / "Assets");
		const auto asset = directory / "Assets/Model.gltf";
		std::ofstream(asset) << "{}";
		auto authored = session.GetMachine().GetConfiguration();
		authored.Components.front().VisualModel = asset.generic_string();
		SaveEditorMachine(authored, directory / "Machine.factory");
		auto relative = LoadConfigurationFile(directory / "Machine.factory");
		Check(relative.GetDefinition(id).VisualModel == "Assets/Model.gltf");
#ifdef _WIN32
		auto crossDrive = authored;
		const auto otherDrive = directory.root_name() == "Z:" ? "Y:/FactoryCore/Model.gltf" : "Z:/FactoryCore/Model.gltf";
		crossDrive.Components.front().VisualModel = otherDrive;
		SaveEditorMachine(crossDrive, directory / "CrossDrive.factory");
		Check(LoadConfigurationFile(directory / "CrossDrive.factory").GetDefinition(id).VisualModel == otherDrive);
#endif
		auto reopened = LoadEditorMachine(directory / "Machine.factory");
		Check(reopened.GetDefinition(id).VisualModel == asset.generic_string());
		auto moved = directory / "Moved";
		std::filesystem::create_directories(moved / "Assets");
		std::filesystem::copy_file(asset, moved / "Assets/Model.gltf", std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(directory / "Machine.factory", moved / "Machine.factory", std::filesystem::copy_options::overwrite_existing);
		Check(LoadEditorMachine(moved / "Machine.factory").GetDefinition(id).VisualModel == (moved / "Assets/Model.gltf").generic_string());
		session.Load(std::move(reopened));
		Check(!session.IsModified() && !session.CanUndo() && session.GetSelection() == 0);
		struct FailingController final : IController
		{
			void Apply(Machine& machine) override { machine.SetInput({ 1, "Extend" }, true); throw std::runtime_error("Controller failed"); }
		};
		session.Play(std::make_unique<FailingController>());
		Throws([&] { session.Advance(.01); });
		Check(!session.IsPlaying() && !session.GetLastError().empty() && session.GetMachine().GetTick() == 0);
		Throws([&] { session.Play(); });
		Throws([&] { session.StepOnce(); });
		session.Stop();
		Check(session.GetLastError().empty() && !session.IsModified());
		std::cout << "PASS editor history, transaction rollback, selection, playback, fixed timing and overload handling\n";
		return 0;
	}
	catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
