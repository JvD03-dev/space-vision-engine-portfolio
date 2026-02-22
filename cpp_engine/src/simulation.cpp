#include "engine/simulation.hpp"

#include <algorithm>
#include <cmath>

#include "engine/math.hpp"

namespace svl {

namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 scale(const Vec3& v, double s) {
    return {v.x * s, v.y * s, v.z * s};
}

Vec3 rotate_x(const Vec3& v, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}

Vec3 orbital_to_inertial(const Vec3& orbital, double inclination_rad, double raan_rad) {
    return rotate_z(rotate_x(orbital, inclination_rad), raan_rad);
}

Vec3 gravitational_accel(const Vec3& position_m, double mu_m3_s2) {
    const double r2 = dot(position_m, position_m);
    if (r2 <= 1e-6) {
        return {0.0, 0.0, 0.0};
    }
    const double inv_r = 1.0 / std::sqrt(r2);
    const double inv_r3 = inv_r * inv_r * inv_r;
    return scale(position_m, -mu_m3_s2 * inv_r3);
}

Vec3 compute_sun_direction(const ScenarioConfig& scenario, double sim_time_s) {
    Vec3 dir = scenario.sun_direction;
    if (scenario.sun_dynamic) {
        const double w = (2.0 * kPi) / std::max(1.0, scenario.sun_annual_period_s);
        dir = rotate_z(dir, w * sim_time_s);
    }
    return normalize(dir);
}

double compute_eclipse_visibility(
    const ScenarioConfig& scenario,
    const Vec3& sat_inertial,
    const Vec3& sun_direction) {
    if (!scenario.enable_eclipse) {
        return 1.0;
    }

    const Vec3 shadow_axis = scale(sun_direction, -1.0);
    const double shadow_distance = dot(sat_inertial, shadow_axis);
    if (shadow_distance <= 0.0) {
        return 1.0;
    }

    const Vec3 axis_projection = scale(shadow_axis, shadow_distance);
    const double radial_distance = norm(sub(sat_inertial, axis_projection));

    const double sun_half_angle_rad =
        (scenario.sun_angular_diameter_deg * 0.5) * (kPi / 180.0);
    const double tan_alpha = std::tan(std::max(1e-6, sun_half_angle_rad));

    const double umbra_radius = scenario.earth_radius_m - shadow_distance * tan_alpha;
    const double penumbra_radius = scenario.earth_radius_m + shadow_distance * tan_alpha;

    if (radial_distance <= umbra_radius) {
        return 0.0;
    }
    if (radial_distance >= penumbra_radius) {
        return 1.0;
    }

    const double span = std::max(1e-6, penumbra_radius - umbra_radius);
    const double t = (radial_distance - umbra_radius) / span;
    return std::clamp(t, 0.0, 1.0);
}

}  // namespace

SimulationState initialize_simulation_state(const ScenarioConfig& scenario) {
    SimulationState state{};
    state.sim_time_s = 0.0;

    const double inclination_rad = scenario.orbit_inclination_deg * (kPi / 180.0);
    const double raan_rad = scenario.orbit_raan_deg * (kPi / 180.0);
    const double earth_tilt_rad = scenario.earth_axial_tilt_deg * (kPi / 180.0);
    const double phase_rad = scenario.orbit_phase_deg * (kPi / 180.0);
    const double radius_m = std::max(scenario.earth_radius_m + 1'000.0, scenario.orbit_radius_m);
    const double mu_m3_s2 = std::max(1.0, scenario.mu_earth_m3_s2);
    const double speed_mps = std::sqrt(mu_m3_s2 / radius_m);

    const Vec3 orbital_pos = {radius_m * std::cos(phase_rad), radius_m * std::sin(phase_rad), 0.0};
    const Vec3 orbital_vel = {-speed_mps * std::sin(phase_rad), speed_mps * std::cos(phase_rad), 0.0};

    state.satellite_position_m = rotate_x(
        orbital_to_inertial(orbital_pos, inclination_rad, raan_rad),
        earth_tilt_rad);
    state.satellite_velocity_mps = rotate_x(
        orbital_to_inertial(orbital_vel, inclination_rad, raan_rad),
        earth_tilt_rad);
    state.initialized = true;
    return state;
}

SimulationState interpolate_simulation_state(
    const SimulationState& a,
    const SimulationState& b,
    double alpha) {
    const double t = std::clamp(alpha, 0.0, 1.0);
    const auto lerp = [t](double x0, double x1) -> double {
        return x0 + (x1 - x0) * t;
    };

    SimulationState out{};
    out.sim_time_s = lerp(a.sim_time_s, b.sim_time_s);
    out.satellite_position_m = {
        lerp(a.satellite_position_m.x, b.satellite_position_m.x),
        lerp(a.satellite_position_m.y, b.satellite_position_m.y),
        lerp(a.satellite_position_m.z, b.satellite_position_m.z),
    };
    out.satellite_velocity_mps = {
        lerp(a.satellite_velocity_mps.x, b.satellite_velocity_mps.x),
        lerp(a.satellite_velocity_mps.y, b.satellite_velocity_mps.y),
        lerp(a.satellite_velocity_mps.z, b.satellite_velocity_mps.z),
    };
    out.initialized = a.initialized || b.initialized;
    return out;
}

void advance_simulation_state(
    const ScenarioConfig& scenario,
    SimulationState& state,
    double dt_s) {
    if (!state.initialized) {
        state = initialize_simulation_state(scenario);
    }
    if (dt_s <= 0.0) {
        return;
    }

    const double step_s = std::max(1e-6, dt_s);
    const double mu_m3_s2 = std::max(1.0, scenario.mu_earth_m3_s2);

    const Vec3 a0 = gravitational_accel(state.satellite_position_m, mu_m3_s2);
    const Vec3 p1 = add(
        state.satellite_position_m,
        add(scale(state.satellite_velocity_mps, step_s), scale(a0, 0.5 * step_s * step_s)));
    const Vec3 a1 = gravitational_accel(p1, mu_m3_s2);
    const Vec3 v1 = add(state.satellite_velocity_mps, scale(add(a0, a1), 0.5 * step_s));

    state.satellite_position_m = p1;
    state.satellite_velocity_mps = v1;
    state.sim_time_s += step_s;
}

SimulationFrame build_simulation_frame(
    const ScenarioConfig& scenario,
    const SimulationState& state,
    double sim_fps_reference) {
    SimulationFrame frame{};

    SimulationState local_state = state;
    if (!local_state.initialized) {
        local_state = initialize_simulation_state(scenario);
    }

    const auto to_render = [&](const Vec3& inertial, const Vec3& anchor) -> Vec3 {
        return scale(sub(inertial, anchor), 1.0 / frame.render_m_per_unit);
    };

    frame.anchor = scenario.anchor;
    frame.render_m_per_unit = std::max(1e-3, scenario.render_m_per_unit);
    frame.inclination_rad = scenario.orbit_inclination_deg * (kPi / 180.0);
    frame.raan_rad = scenario.orbit_raan_deg * (kPi / 180.0);
    const double earth_tilt_rad = scenario.earth_axial_tilt_deg * (kPi / 180.0);

    const Vec3 earth_inertial = {0.0, 0.0, 0.0};
    const Vec3 sat_inertial = local_state.satellite_position_m;
    const Vec3 sat_velocity = local_state.satellite_velocity_mps;
    const Vec3 ref_p = rotate_x(
        orbital_to_inertial({1.0, 0.0, 0.0}, frame.inclination_rad, frame.raan_rad),
        earth_tilt_rad);
    const Vec3 ref_q = rotate_x(
        orbital_to_inertial({0.0, 1.0, 0.0}, frame.inclination_rad, frame.raan_rad),
        earth_tilt_rad);

    frame.orbital_normal = normalize(cross(ref_p, ref_q));
    if (norm(frame.orbital_normal) <= 1e-12) {
        frame.orbital_normal = {0.0, 0.0, 1.0};
    }

    Vec3 sat_plane = sub(
        sat_inertial,
        scale(frame.orbital_normal, dot(sat_inertial, frame.orbital_normal)));
    if (norm(sat_plane) <= 1e-9) {
        sat_plane = sat_inertial;
    }
    frame.satellite_radial = normalize(sat_plane);

    Vec3 tangent = normalize(cross(frame.orbital_normal, frame.satellite_radial));
    if (dot(tangent, sat_velocity) < 0.0) {
        tangent = scale(tangent, -1.0);
    }
    if (norm(tangent) <= 1e-12) {
        tangent = normalize(sat_velocity);
    }
    frame.satellite_tangent = tangent;

    const Vec3 camera_inertial = add(
        sat_inertial,
        add(
            add(
                scale(frame.satellite_tangent, -scenario.camera_trailing_distance_m),
                scale(frame.satellite_radial, scenario.camera_radial_offset_m)),
            scale(frame.orbital_normal, scenario.camera_normal_offset_m)));

    Vec3 anchor_inertial = earth_inertial;
    if (scenario.anchor == AnchorFrame::Satellite) {
        anchor_inertial = sat_inertial;
    } else if (scenario.anchor == AnchorFrame::Camera) {
        anchor_inertial = camera_inertial;
    }

    const double sat_radius_m = norm(sat_inertial);
    frame.earth_render_radius = scenario.earth_radius_m / frame.render_m_per_unit;
    frame.orbit_render_radius = std::max(scenario.orbit_radius_m, sat_radius_m) / frame.render_m_per_unit;
    frame.satellite_render_size = std::max(0.25, scenario.target_radius_m / frame.render_m_per_unit);

    frame.earth_position = to_render(earth_inertial, anchor_inertial);
    frame.satellite_position = to_render(sat_inertial, anchor_inertial);
    frame.camera_position = to_render(camera_inertial, anchor_inertial);
    frame.camera_target = frame.satellite_position;

    frame.theta = std::atan2(dot(frame.satellite_radial, ref_q), dot(frame.satellite_radial, ref_p));

    frame.sun_direction = compute_sun_direction(scenario, local_state.sim_time_s);
    frame.sun_visibility = compute_eclipse_visibility(scenario, sat_inertial, frame.sun_direction);
    frame.in_eclipse = frame.sun_visibility < 0.999;
    frame.ambient_floor = scenario.ambient_floor;
    frame.exposure_scale = scenario.camera_exposure_scale;
    frame.camera_track_horizon_lock = scenario.camera_track_horizon_lock;
    frame.show_sun_marker = scenario.show_sun_marker;
    frame.earth_axial_tilt_deg = scenario.earth_axial_tilt_deg;
    frame.earth_spin_deg = scenario.earth_spin_deg_per_frame * sim_fps_reference * local_state.sim_time_s;
    frame.fov_y_deg = std::clamp(
        2.0 * std::atan(24.0 / (2.0 * scenario.lens_mm)) * (180.0 / kPi),
        18.0,
        90.0);
    frame.default_camera_distance = std::max(
        std::max(6.0, frame.orbit_render_radius * 1.6),
        frame.earth_render_radius * 1.7);

    frame.sat_earth_distance_m = sat_radius_m;
    frame.altitude_m = frame.sat_earth_distance_m - scenario.earth_radius_m;
    frame.orbital_speed_mps = norm(sat_velocity);
    frame.camera_sat_distance_m = norm(sub(camera_inertial, sat_inertial));

    const double mu_m3_s2 = std::max(1.0, scenario.mu_earth_m3_s2);
    const double specific_energy = 0.5 * frame.orbital_speed_mps * frame.orbital_speed_mps - mu_m3_s2 / std::max(1.0, sat_radius_m);
    if (specific_energy < 0.0) {
        const double semi_major_axis_m = -mu_m3_s2 / (2.0 * specific_energy);
        frame.orbital_period_s = 2.0 * kPi * std::sqrt((semi_major_axis_m * semi_major_axis_m * semi_major_axis_m) / mu_m3_s2);
    } else {
        frame.orbital_period_s = 0.0;
    }

    return frame;
}

}  // namespace svl
