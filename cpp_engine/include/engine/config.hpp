#pragma once

#include <filesystem>

#include "engine/types.hpp"

namespace svl {

ScenarioConfig load_scenario_config(const std::filesystem::path& path);
CliArgs parse_cli_args(int argc, char** argv);

}  // namespace svl

