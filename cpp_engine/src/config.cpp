#include "engine/config.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <cctype>

#include "engine/math.hpp"
#include "nlohmann/json.hpp"

namespace svl {

namespace {

using json = nlohmann::json;

const json* find_path(const json& root, std::initializer_list<const char*> keys) {
    const json* cur = &root;
    for (const char* key : keys) {
        if (!cur->is_object() || !cur->contains(key)) {
            return nullptr;
        }
        cur = &((*cur)[key]);
    }
    return cur;
}

std::string joined_path(std::initializer_list<const char*> keys) {
    std::string path;
    bool first = true;
    for (const char* key : keys) {
        if (!first) {
            path += ".";
        }
        path += key;
        first = false;
    }
    return path;
}

void assign_number_if_present(
    const json& root,
    std::initializer_list<const char*> keys,
    double& out_value) {
    const json* ptr = find_path(root, keys);
    if (ptr == nullptr) {
        return;
    }
    if (!ptr->is_number()) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must be a number.");
    }
    out_value = ptr->get<double>();
}

void assign_vec3_if_present(
    const json& root,
    std::initializer_list<const char*> keys,
    Vec3& out_value) {
    const json* ptr = find_path(root, keys);
    if (ptr == nullptr) {
        return;
    }
    if (!ptr->is_array() || ptr->size() != 3U) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must be a 3-element array.");
    }
    if (!(*ptr)[0].is_number() || !(*ptr)[1].is_number() || !(*ptr)[2].is_number()) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must contain only numbers.");
    }
    out_value = {(*ptr)[0].get<double>(), (*ptr)[1].get<double>(), (*ptr)[2].get<double>()};
}

void assign_path_if_present(
    const json& root,
    std::initializer_list<const char*> keys,
    std::filesystem::path& out_value) {
    const json* ptr = find_path(root, keys);
    if (ptr == nullptr) {
        return;
    }
    if (!ptr->is_string()) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must be a string path.");
    }
    out_value = ptr->get<std::string>();
}

void assign_bool_if_present(
    const json& root,
    std::initializer_list<const char*> keys,
    bool& out_value) {
    const json* ptr = find_path(root, keys);
    if (ptr == nullptr) {
        return;
    }
    if (!ptr->is_boolean()) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must be a boolean.");
    }
    out_value = ptr->get<bool>();
}

void assign_anchor_if_present(
    const json& root,
    std::initializer_list<const char*> keys,
    AnchorFrame& out_anchor) {
    const json* ptr = find_path(root, keys);
    if (ptr == nullptr) {
        return;
    }
    if (!ptr->is_string()) {
        throw std::runtime_error("Config field '" + joined_path(keys) + "' must be a string.");
    }

    std::string value = ptr->get<std::string>();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "earth") {
        out_anchor = AnchorFrame::Earth;
        return;
    }
    if (value == "target" || value == "satellite") {
        out_anchor = AnchorFrame::Satellite;
        return;
    }
    if (value == "camera") {
        out_anchor = AnchorFrame::Camera;
        return;
    }

    throw std::runtime_error(
        "Config field '" + joined_path(keys) +
        "' must be one of: earth, satellite/target, camera.");
}

double parse_positive_arg(const std::string& value, const char* arg_name) {
    try {
        const double parsed = std::stod(value);
        if (parsed <= 0.0) {
            throw std::runtime_error("");
        }
        return parsed;
    } catch (...) {
        throw std::runtime_error(std::string("Argument ") + arg_name + " expects a positive number.");
    }
}

}  // namespace

ScenarioConfig load_scenario_config(const std::filesystem::path& path) {
    ScenarioConfig config{};
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Could not open config file: " + path.string());
    }

    json root;
    try {
        input >> root;
    } catch (const std::exception& ex) {
        throw std::runtime_error(
            "Failed to parse JSON in config file '" + path.string() + "': " + ex.what());
    }

    assign_number_if_present(root, {"earth", "radius"}, config.earth_radius_m);
    assign_number_if_present(root, {"earth", "axial_tilt_deg"}, config.earth_axial_tilt_deg);
    assign_number_if_present(root, {"earth", "rotation_deg_per_frame"}, config.earth_spin_deg_per_frame);
    assign_path_if_present(root, {"earth", "texture_day"}, config.earth_texture_day);
    assign_number_if_present(root, {"target", "radius"}, config.target_radius_m);
    assign_number_if_present(root, {"camera", "lens_mm"}, config.lens_mm);
    assign_number_if_present(root, {"camera", "exposure_scale"}, config.camera_exposure_scale);
    assign_bool_if_present(root, {"camera", "track_horizon_lock"}, config.camera_track_horizon_lock);
    assign_number_if_present(root, {"dynamics", "mu_earth"}, config.mu_earth_m3_s2);
    assign_number_if_present(root, {"dynamics", "orbit_radius"}, config.orbit_radius_m);
    assign_number_if_present(root, {"dynamics", "orbit_period_frames"}, config.orbit_period_frames);
    assign_number_if_present(root, {"dynamics", "orbit_inclination_deg"}, config.orbit_inclination_deg);
    assign_number_if_present(root, {"dynamics", "orbit_raan_deg"}, config.orbit_raan_deg);
    assign_number_if_present(root, {"dynamics", "orbit_phase_deg"}, config.orbit_phase_deg);
    assign_number_if_present(root, {"dynamics", "camera_trailing_distance"}, config.camera_trailing_distance_m);
    assign_number_if_present(root, {"dynamics", "camera_radial_offset"}, config.camera_radial_offset_m);
    assign_number_if_present(root, {"dynamics", "camera_normal_offset"}, config.camera_normal_offset_m);
    assign_number_if_present(root, {"dynamics", "render_m_per_unit"}, config.render_m_per_unit);
    assign_number_if_present(root, {"dynamics", "sun_annual_period_s"}, config.sun_annual_period_s);
    assign_number_if_present(root, {"dynamics", "sun_angular_diameter_deg"}, config.sun_angular_diameter_deg);
    assign_number_if_present(root, {"sun", "angle_deg"}, config.sun_angular_diameter_deg);
    assign_number_if_present(root, {"dynamics", "ambient_floor"}, config.ambient_floor);
    assign_bool_if_present(root, {"dynamics", "sun_dynamic"}, config.sun_dynamic);
    assign_bool_if_present(root, {"dynamics", "enable_eclipse"}, config.enable_eclipse);
    assign_bool_if_present(root, {"dynamics", "show_sun_marker"}, config.show_sun_marker);
    assign_anchor_if_present(root, {"dynamics", "anchor"}, config.anchor);
    assign_vec3_if_present(root, {"dynamics", "sun_direction"}, config.sun_direction);

    config.mu_earth_m3_s2 = std::max(1.0, config.mu_earth_m3_s2);
    config.earth_axial_tilt_deg = std::clamp(config.earth_axial_tilt_deg, -90.0, 90.0);
    config.orbit_period_frames = std::max(1.0, config.orbit_period_frames);
    config.lens_mm = std::max(10.0, config.lens_mm);
    config.camera_exposure_scale = std::max(0.0, config.camera_exposure_scale);
    config.sun_annual_period_s = std::max(1.0, config.sun_annual_period_s);
    config.sun_angular_diameter_deg = std::clamp(config.sun_angular_diameter_deg, 0.01, 5.0);
    config.ambient_floor = std::clamp(config.ambient_floor, 0.0, 1.0);
    config.render_m_per_unit = std::max(1e-3, config.render_m_per_unit);
    config.sun_direction = normalize(config.sun_direction);

    return config;
}

CliArgs parse_cli_args(int argc, char** argv) {
    CliArgs args{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            args.config_path = argv[++i];
            continue;
        }
        if (arg == "--speed" && i + 1 < argc) {
            args.speed = parse_positive_arg(argv[++i], "--speed");
            continue;
        }
        if (arg == "--fps" && i + 1 < argc) {
            args.fps = parse_positive_arg(argv[++i], "--fps");
            continue;
        }
        if (arg == "-h" || arg == "--help") {
            throw std::runtime_error(
                "USAGE_ONLY\nUsage: space_vision_engine [--config PATH] [--speed N] [--fps N]\n"
                "  --config  Scenario JSON path.\n"
                "  --speed   Simulation speed multiplier (default 80).\n"
                "  --fps     Simulation fixed-step rate in Hz (default 60).\n");
        }
    }

    return args;
}

}  // namespace svl
