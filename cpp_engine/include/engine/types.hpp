#pragma once

#include <filesystem>
#include <string>

namespace svl {

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct UiRect {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

enum class AnchorFrame {
    Earth,
    Satellite,
    Camera,
};

struct ScenarioConfig {
    double earth_radius_m = 6'371'000.0;
    double earth_axial_tilt_deg = 23.44;
    double earth_spin_deg_per_frame = 0.02;
    std::filesystem::path earth_texture_day{};
    double target_radius_m = 1.2;
    double mu_earth_m3_s2 = 3.986004418e14;
    double orbit_radius_m = 6'771'000.0;
    double orbit_period_frames = 1200.0;
    double orbit_inclination_deg = 51.6;
    double orbit_raan_deg = 25.0;
    double orbit_phase_deg = 0.0;
    double camera_trailing_distance_m = 35.0;
    double camera_radial_offset_m = 8.0;
    double camera_normal_offset_m = 6.0;
    double render_m_per_unit = 100000.0;
    AnchorFrame anchor = AnchorFrame::Earth;
    double lens_mm = 70.0;
    double camera_exposure_scale = 1.0;
    bool camera_track_horizon_lock = true;
    Vec3 sun_direction = {1.0, 0.22, 0.12};
    bool sun_dynamic = true;
    double sun_annual_period_s = 31'557'600.0;
    double sun_angular_diameter_deg = 0.53;
    bool enable_eclipse = true;
    double ambient_floor = 0.04;
    bool show_sun_marker = false;
};

struct SimulationState {
    double sim_time_s = 0.0;
    Vec3 satellite_position_m{};
    Vec3 satellite_velocity_mps{};
    bool initialized = false;
};

struct CliArgs {
    std::filesystem::path config_path = "configs/scenarios/rendezvous_glint_fast_iter.json";
    double speed = 80.0;
    double fps = 60.0;
};

enum class InputEventType {
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseWheel,
    Resize,
    CloseRequested,
};

enum class MouseButton {
    Unknown,
    Left,
    Middle,
    Right,
};

struct InputEvent {
    InputEventType type = InputEventType::KeyDown;
    int key = 0;
    MouseButton mouse_button = MouseButton::Unknown;
    int mouse_x = 0;
    int mouse_y = 0;
    int wheel_delta = 0;
    int width = 0;
    int height = 0;
    bool is_repeat = false;
};

struct RuntimeState {
    bool running = true;
    bool paused = false;
    bool wireframe = false;
    bool show_orbit = true;
    bool show_debug_guides = false;
    double speed = 80.0;
    double sim_fps_reference = 60.0;
    int viewport_width = 1280;
    int viewport_height = 720;
};

struct SimulationFrame {
    double inclination_rad = 0.0;
    double raan_rad = 0.0;
    double theta = 0.0;
    double earth_render_radius = 1.0;
    double orbit_render_radius = 1.0;
    double satellite_render_size = 0.1;
    Vec3 earth_position{};
    Vec3 satellite_position{};
    Vec3 camera_position{};
    Vec3 camera_target{};
    Vec3 satellite_radial{0.0, 0.0, 1.0};
    Vec3 satellite_tangent{1.0, 0.0, 0.0};
    Vec3 orbital_normal{0.0, 0.0, 1.0};
    Vec3 sun_direction{1.0, 0.0, 0.0};
    AnchorFrame anchor = AnchorFrame::Earth;
    bool in_eclipse = false;
    double sun_visibility = 1.0;
    double ambient_floor = 0.04;
    double exposure_scale = 1.0;
    bool camera_track_horizon_lock = false;
    bool show_sun_marker = false;
    double earth_axial_tilt_deg = 23.44;
    double earth_spin_deg = 0.0;
    double fov_y_deg = 55.0;
    double default_camera_distance = 5.0;
    double render_m_per_unit = 100000.0;

    double sat_earth_distance_m = 0.0;
    double altitude_m = 0.0;
    double orbital_period_s = 0.0;
    double orbital_speed_mps = 0.0;
    double camera_sat_distance_m = 0.0;
};

enum class CameraModeType {
    FreeOrbit,
    TrackSatellite,
};

struct CameraPose {
    Vec3 eye{};
    Vec3 target{};
    Vec3 up{0.0, 0.0, 1.0};
};

}  // namespace svl
