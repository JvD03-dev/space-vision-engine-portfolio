#pragma once

#include <vector>

#include "engine/types.hpp"

namespace svl {

void initialize_gl_state(const ScenarioConfig& scenario);
std::vector<Vec3> build_star_field(int count, double radius);
void render_scene(
    const RuntimeState& runtime,
    const SimulationFrame& frame,
    const CameraPose& camera_pose,
    const std::vector<Vec3>& stars,
    CameraModeType camera_mode);

}  // namespace svl
