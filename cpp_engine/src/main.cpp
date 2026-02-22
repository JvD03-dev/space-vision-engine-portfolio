#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "engine/camera.hpp"
#include "engine/config.hpp"
#include "engine/renderer_gl_fixed.hpp"
#include "engine/simulation.hpp"
#include "engine/ui.hpp"
#include "platform/win32_window.hpp"

namespace svl {

namespace {

bool is_usage_only_error(const std::exception& ex) {
    return std::string(ex.what()).rfind("USAGE_ONLY", 0) == 0;
}

std::string controls_help() {
    return "[cpp-engine] controls: SPACE pause | W wireframe | O orbit | UP/DOWN speed | "
           "A/D yaw | R/F pitch | Z/X zoom | MMB-drag rotate | wheel zoom | C toggle track | "
           "G debug guides | "
           "LMB camera icon track | ESC quit";
}

const char* anchor_name(AnchorFrame anchor) {
    switch (anchor) {
        case AnchorFrame::Earth:
            return "earth";
        case AnchorFrame::Satellite:
            return "satellite";
        case AnchorFrame::Camera:
            return "camera";
        default:
            return "unknown";
    }
}

}  // namespace

}  // namespace svl

int main(int argc, char** argv) {
    using namespace svl;

    try {
        CliArgs cli = parse_cli_args(argc, argv);
        const ScenarioConfig scenario = load_scenario_config(cli.config_path);

        RuntimeState runtime{};
        runtime.speed = cli.speed;
        runtime.sim_fps_reference = cli.fps;
        const double fixed_step_s = 1.0 / std::max(1.0, runtime.sim_fps_reference);

        CameraController camera;
        Win32Window window;
        const HINSTANCE instance = GetModuleHandleA(nullptr);
        if (!window.create(instance, "Space Vision Engine (C++)", runtime.viewport_width, runtime.viewport_height, [&](const InputEvent& event) {
                if (event.type == InputEventType::Resize) {
                    runtime.viewport_width = std::max(1, event.width);
                    runtime.viewport_height = std::max(1, event.height);
                    return;
                }

                if (event.type == InputEventType::CloseRequested) {
                    runtime.running = false;
                    return;
                }

                if (event.type == InputEventType::KeyDown) {
                    if (event.key == VK_ESCAPE) {
                        window.request_close();
                        runtime.running = false;
                        return;
                    }
                    if (event.key == VK_SPACE && !event.is_repeat) {
                        runtime.paused = !runtime.paused;
                        std::cout << "[cpp-engine] pause=" << (runtime.paused ? "on" : "off") << "\n";
                        return;
                    }
                    if (event.key == 'W' && !event.is_repeat) {
                        runtime.wireframe = !runtime.wireframe;
                        return;
                    }
                    if (event.key == 'O' && !event.is_repeat) {
                        runtime.show_orbit = !runtime.show_orbit;
                        return;
                    }
                    if (event.key == 'G' && !event.is_repeat) {
                        runtime.show_debug_guides = !runtime.show_debug_guides;
                        return;
                    }
                    if (event.key == VK_UP) {
                        runtime.speed *= 1.2;
                        return;
                    }
                    if (event.key == VK_DOWN) {
                        runtime.speed = std::max(0.01, runtime.speed / 1.2);
                        return;
                    }
                    if (event.key == 'C' && !event.is_repeat) {
                        camera.toggle_mode();
                        return;
                    }
                }

                if (event.type == InputEventType::MouseButtonDown &&
                    event.mouse_button == MouseButton::Left) {
                    const UiRect button = camera_button_rect(runtime.viewport_width, runtime.viewport_height);
                    if (rect_contains(button, event.mouse_x, event.mouse_y)) {
                        camera.toggle_mode();
                        return;
                    }
                }

                camera.on_input(event);
            })) {
            throw std::runtime_error("Failed to initialize Win32 OpenGL window/context.");
        }

        initialize_gl_state(scenario);
        const double render_scale = std::max(1e-3, scenario.render_m_per_unit);
        const double orbit_render_radius = scenario.orbit_radius_m / render_scale;
        const double star_radius = std::max(2500.0, orbit_render_radius * 24.0);
        const std::vector<Vec3> stars = build_star_field(2500, star_radius);

        std::cout << controls_help() << "\n";
        std::cout << "[cpp-engine] config=" << cli.config_path.string()
                  << " speed=" << runtime.speed
                  << " fixed_dt=" << fixed_step_s << "s"
                  << " render_m_per_unit=" << scenario.render_m_per_unit
                  << " anchor=" << anchor_name(scenario.anchor) << "\n";

        auto previous = std::chrono::high_resolution_clock::now();
        double sim_accumulator_s = 0.0;
        double title_accum_s = 0.0;
        SimulationState sim_state = initialize_simulation_state(scenario);
        SimulationState sim_state_prev = sim_state;
        SimulationFrame frame{};

        while (runtime.running && !window.should_close()) {
            if (!window.pump_events()) {
                runtime.running = false;
                break;
            }

            const auto now = std::chrono::high_resolution_clock::now();
            const std::chrono::duration<double> dt = now - previous;
            previous = now;
            const double dt_s = std::clamp(dt.count(), 0.0, 0.1);

            if (!runtime.paused) {
                sim_accumulator_s += dt_s * runtime.speed;
            }

            int substeps = 0;
            constexpr int kMaxSubstepsPerFrame = 4096;
            while (sim_accumulator_s >= fixed_step_s && substeps < kMaxSubstepsPerFrame) {
                sim_state_prev = sim_state;
                advance_simulation_state(scenario, sim_state, fixed_step_s);
                sim_accumulator_s -= fixed_step_s;
                ++substeps;
            }
            if (substeps == kMaxSubstepsPerFrame) {
                sim_accumulator_s = std::fmod(sim_accumulator_s, fixed_step_s);
            }

            const double interpolation_alpha = std::clamp(
                sim_accumulator_s / fixed_step_s,
                0.0,
                1.0);
            const SimulationState render_state = interpolate_simulation_state(
                sim_state_prev,
                sim_state,
                interpolation_alpha);

            frame = build_simulation_frame(
                scenario,
                render_state,
                runtime.sim_fps_reference);
            camera.update(frame, dt_s);

            render_scene(
                runtime,
                frame,
                camera.current_pose(),
                stars,
                camera.mode());
            window.swap_buffers();
            Sleep(1);

            title_accum_s += dt_s;
            if (title_accum_s > 0.5) {
                title_accum_s = 0.0;
                std::ostringstream title;
                title << "Space Vision Engine (C++) | speed x" << runtime.speed
                      << (runtime.paused ? " | PAUSED" : " | RUNNING")
                      << " | anchor=" << anchor_name(frame.anchor)
                      << " scale=" << frame.render_m_per_unit << "m/u"
                      << " | alt=" << (frame.altitude_m / 1000.0) << "km"
                      << " v=" << (frame.orbital_speed_mps / 1000.0) << "km/s"
                      << " T=" << (frame.orbital_period_s / 60.0) << "min"
                      << " sun=" << (frame.sun_visibility * 100.0) << "%"
                      << (frame.in_eclipse ? " eclipse" : "")
                      << " cam-dist=" << frame.camera_sat_distance_m << "m"
                      << " | cam=" << (camera.mode() == CameraModeType::TrackSatellite ? "track" : "free")
                      << " | " << camera.debug_text();
                window.set_title(title.str());
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        if (is_usage_only_error(ex)) {
            std::string usage = ex.what();
            const std::string prefix = "USAGE_ONLY\n";
            if (usage.rfind(prefix, 0) == 0) {
                usage = usage.substr(prefix.size());
            }
            std::cout << usage << "\n";
            return 0;
        }
        std::cerr << "[cpp-engine] error: " << ex.what() << "\n";
        return 1;
    }
}
