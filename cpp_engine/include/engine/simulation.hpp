#pragma once

#include "engine/types.hpp"

namespace svl {

SimulationState initialize_simulation_state(const ScenarioConfig& scenario);
SimulationState interpolate_simulation_state(
    const SimulationState& a,
    const SimulationState& b,
    double alpha);
void advance_simulation_state(
    const ScenarioConfig& scenario,
    SimulationState& state,
    double dt_s);

SimulationFrame build_simulation_frame(
    const ScenarioConfig& scenario,
    const SimulationState& state,
    double sim_fps_reference);

}  // namespace svl
