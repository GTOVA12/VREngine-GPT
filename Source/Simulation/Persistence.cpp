#include "FactoryCore/Persistence.h"

#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace FactoryCore
{
	namespace
	{
		constexpr std::size_t s_MaxFileBytes = 8 * 1024 * 1024;

		void Expect(std::istream& input, const char* expected)
		{
			std::string token;
			if (!(input >> token) || token != expected)
				throw std::invalid_argument(std::string("Expected configuration token: ") + expected);
		}

		ComponentId ReadId(std::istream& input)
		{
			std::string token;
			if (!(input >> token) || token.empty()
				|| token.find_first_not_of("0123456789") != std::string::npos)
				throw std::invalid_argument("Component ID must be an unsigned decimal integer");
			ComponentId id = 0;
			const auto result = std::from_chars(token.data(), token.data() + token.size(), id);
			if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
				throw std::invalid_argument("Component ID is out of range");
			return id;
		}

		void WriteVector(std::ostream& output, const Vector3& value)
		{
			output << value.X << ' ' << value.Y << ' ' << value.Z << ' ';
		}

		void ReadVector(std::istream& input, Vector3& value)
		{
			if (!(input >> value.X >> value.Y >> value.Z))
				throw std::invalid_argument("Invalid transform vector");
		}

		// A private sibling directory makes temporary file creation collision-safe.
		class TemporaryFile
		{
		public:
			explicit TemporaryFile(const std::filesystem::path& target)
			{
				std::random_device random;
				for (int attempt = 0; attempt < 32; ++attempt)
				{
					m_Directory = target;
					m_Directory += ".tmp." + std::to_string(random());
					if (std::filesystem::create_directory(m_Directory))
					{
						m_Path = m_Directory / "Configuration";
						return;
					}
				}
				throw std::runtime_error("Cannot allocate a temporary configuration file");
			}

			~TemporaryFile()
			{
				std::error_code ignored;
				std::filesystem::remove(m_Path, ignored);
				std::filesystem::remove(m_Directory, ignored);
			}

			const std::filesystem::path& GetPath() const { return m_Path; }

		private:
			std::filesystem::path m_Directory;
			std::filesystem::path m_Path;
		};
	}

	void SaveConfiguration(const Machine& machine, std::ostream& output)
	{
		const auto definition = machine.GetConfiguration();
		std::ostringstream text;
		text.imbue(std::locale::classic());
		text << std::setprecision(std::numeric_limits<double>::max_digits10);
		text << "FACTORYCORE 1\nmachine " << std::quoted(definition.Name) << ' ' << definition.TimeStep
			<< "\ncomponents " << definition.Components.size() << '\n';
		for (const auto& component : definition.Components)
		{
			text << "component " << component.Id << ' ' << std::quoted(component.Name) << ' '
				<< ToString(component.Kind) << ' ' << std::quoted(component.VisualModel) << ' ';
			WriteVector(text, component.Placement.Position);
			WriteVector(text, component.Placement.Rotation);
			WriteVector(text, component.Placement.Scale);
			const auto& properties = component.Properties;
			text << properties.Stroke << ' ' << properties.Speed << ' ' << properties.Acceleration << ' '
				<< properties.Delay << ' ' << properties.Threshold << ' ' << properties.Hysteresis << ' '
				<< properties.InitialPosition << ' ' << properties.Mass << '\n';
		}
		text << "connections " << definition.Connections.size() << '\n';
		for (const auto& connection : definition.Connections)
			text << "connection " << connection.Source.Component << ' ' << std::quoted(connection.Source.Signal)
				<< ' ' << connection.Destination.Component << ' ' << std::quoted(connection.Destination.Signal) << '\n';
		text << "end\n";
		const auto bytes = text.str();
		if (bytes.size() > s_MaxFileBytes)
			throw std::length_error("Configuration exceeds file size limit");
		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		if (!output)
			throw std::runtime_error("Failed to write configuration");
	}

	Machine LoadConfiguration(std::istream& input)
	{
		std::string bytes;
		std::array<char, 4096> buffer{};
		while (input)
		{
			input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			const auto count = static_cast<std::size_t>(input.gcount());
			if (bytes.size() + count > s_MaxFileBytes)
				throw std::length_error("Configuration exceeds file size limit");
			bytes.append(buffer.data(), count);
		}
		if (!input.eof())
			throw std::runtime_error("Failed to read configuration");
		std::istringstream parser(bytes);
		parser.imbue(std::locale::classic());
		Expect(parser, "FACTORYCORE");
		int version = 0;
		if (!(parser >> version) || version != 1)
			throw std::invalid_argument("Unsupported FactoryCore configuration version");
		MachineDefinition definition;
		Expect(parser, "machine");
		if (!(parser >> std::quoted(definition.Name) >> definition.TimeStep))
			throw std::invalid_argument("Invalid machine header");
		Expect(parser, "components");
		const auto componentCount = ReadId(parser);
		if (componentCount > 10000)
			throw std::length_error("Component limit exceeded");
		for (ComponentId index = 0; index < componentCount; ++index)
		{
			Expect(parser, "component");
			ComponentDefinition component;
			component.Id = ReadId(parser);
			std::string kind;
			if (!(parser >> std::quoted(component.Name) >> kind >> std::quoted(component.VisualModel)))
				throw std::invalid_argument("Invalid component header");
			component.Kind = ParseEquipmentKind(kind);
			ReadVector(parser, component.Placement.Position);
			ReadVector(parser, component.Placement.Rotation);
			ReadVector(parser, component.Placement.Scale);
			auto& properties = component.Properties;
			if (!(parser >> properties.Stroke >> properties.Speed >> properties.Acceleration >> properties.Delay
				>> properties.Threshold >> properties.Hysteresis >> properties.InitialPosition >> properties.Mass))
				throw std::invalid_argument("Invalid equipment properties");
			definition.Components.push_back(std::move(component));
		}
		Expect(parser, "connections");
		const auto connectionCount = ReadId(parser);
		if (connectionCount > 100000)
			throw std::length_error("Connection limit exceeded");
		for (ComponentId index = 0; index < connectionCount; ++index)
		{
			Expect(parser, "connection");
			Connection connection;
			connection.Source.Component = ReadId(parser);
			if (!(parser >> std::quoted(connection.Source.Signal)))
				throw std::invalid_argument("Invalid source signal");
			connection.Destination.Component = ReadId(parser);
			if (!(parser >> std::quoted(connection.Destination.Signal)))
				throw std::invalid_argument("Invalid destination signal");
			definition.Connections.push_back(std::move(connection));
		}
		Expect(parser, "end");
		parser >> std::ws;
		if (!parser.eof())
			throw std::invalid_argument("Unexpected data after configuration");
		return Machine::FromConfiguration(definition);
	}

	void SaveConfigurationFile(const Machine& machine, const std::filesystem::path& path)
	{
		TemporaryFile temporary(path);
		{
			std::ofstream output(temporary.GetPath(), std::ios::binary | std::ios::trunc);
			if (!output)
				throw std::runtime_error("Cannot open temporary configuration file");
			SaveConfiguration(machine, output);
			output.flush();
			if (!output)
				throw std::runtime_error("Cannot flush configuration file");
			output.close();
			if (!output)
				throw std::runtime_error("Cannot close configuration file");
		}
#ifdef _WIN32
		if (!MoveFileExW(temporary.GetPath().c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot replace configuration file");
#else
		std::filesystem::rename(temporary.GetPath(), path);
#endif
	}

	Machine LoadConfigurationFile(const std::filesystem::path& path)
	{
		std::ifstream input(path, std::ios::binary);
		if (!input)
			throw std::runtime_error("Cannot open configuration file");
		return LoadConfiguration(input);
	}
}
