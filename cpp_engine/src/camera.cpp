#define NOMINMAX
#include <Windows.h>

#include "engine/camera.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>

#include "engine/math.hpp"

namespace svl {

namespace {

constexpr double kPi = 3.14159265358979323846;

class FreeOrbitCameraMode final : public ICameraMode {
public:
    void on_input(const InputEvent& event) override {
        if (event.type == InputEventType::KeyDown) {
            switch (event.key) {
                case 'A':
                    yaw_deg_ -= 4.0;
                    break;
                case 'D':
                    yaw_deg_ += 4.0;
                    break;
                case 'R':
                    pitch_deg_ = std::min(85.0, pitch_deg_ + 3.0);
                    break;
                case 'F':
                    pitch_deg_ = std::max(-85.0, pitch_deg_ - 3.0);
                    break;
                case 'Z':
                    distance_scale_ = std::max(0.35, distance_scale_ * 0.90);
                    break;
                case 'X':
                    distance_scale_ = std::min(8.0, distance_scale_ * 1.10);
                    break;
                default:
                    break;
            }
            return;
        }

        if (event.type == InputEventType::MouseButtonDown && event.mouse_button == MouseButton::Middle) {
            middle_mouse_down_ = true;
            last_mouse_x_ = event.mouse_x;
            last_mouse_y_ = event.mouse_y;
            return;
        }
        if (event.type == InputEventType::MouseButtonUp && event.mouse_button == MouseButton::Middle) {
            middle_mouse_down_ = false;
            return;
        }
        if (event.type == InputEventType::MouseMove && middle_mouse_down_) {
            const int dx = event.mouse_x - last_mouse_x_;
            const int dy = event.mouse_y - last_mouse_y_;
            last_mouse_x_ = event.mouse_x;
            last_mouse_y_ = event.mouse_y;

            yaw_deg_ += static_cast<double>(dx) * 0.30;
            pitch_deg_ = std::clamp(
                pitch_deg_ - static_cast<double>(dy) * 0.25,
                -85.0,
                85.0);
            return;
        }
        if (event.type == InputEventType::MouseWheel) {
            if (event.wheel_delta > 0) {
                distance_scale_ = std::max(0.35, distance_scale_ * 0.90);
            } else if (event.wheel_delta < 0) {
                distance_scale_ = std::min(8.0, distance_scale_ * 1.10);
            }
        }
    }

    void update(const SimulationFrame& frame, double) override {
        const double distance = frame.default_camera_distance * distance_scale_;
        const double yaw_rad = yaw_deg_ * (kPi / 180.0);
        const double pitch_rad = pitch_deg_ * (kPi / 180.0);

        pose_.target = {0.0, 0.0, 0.0};
        pose_.eye = {
            pose_.target.x + distance * std::cos(pitch_rad) * std::cos(yaw_rad),
            pose_.target.y + distance * std::cos(pitch_rad) * std::sin(yaw_rad),
            pose_.target.z + distance * std::sin(pitch_rad),
        };
        pose_.up = {0.0, 0.0, 1.0};
    }

    CameraPose pose() const override {
        return pose_;
    }

    std::string debug_text() const override {
        std::ostringstream out;
        out << "free:yaw=" << yaw_deg_ << " pitch=" << pitch_deg_ << " zoom=" << distance_scale_;
        return out.str();
    }

private:
    CameraPose pose_{};
    double yaw_deg_ = -90.0;
    double pitch_deg_ = 25.0;
    double distance_scale_ = 1.0;
    bool middle_mouse_down_ = false;
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
};

class TrackSatelliteCameraMode final : public ICameraMode {
public:
    void on_input(const InputEvent& event) override {
        if (event.type == InputEventType::KeyDown) {
            if (event.key == 'Z') {
                distance_scale_ = std::max(0.35, distance_scale_ * 0.90);
            } else if (event.key == 'X') {
                distance_scale_ = std::min(8.0, distance_scale_ * 1.10);
            }
            return;
        }

        if (event.type == InputEventType::MouseWheel) {
            if (event.wheel_delta > 0) {
                distance_scale_ = std::max(0.35, distance_scale_ * 0.90);
            } else if (event.wheel_delta < 0) {
                distance_scale_ = std::min(8.0, distance_scale_ * 1.10);
            }
        }
    }

    void update(const SimulationFrame& frame, double) override {
        Vec3 up_ref = frame.orbital_normal;
        if (frame.camera_track_horizon_lock) {
            up_ref = frame.satellite_radial;
        }
        if (norm(up_ref) <= 1e-12) {
            up_ref = {0.0, 0.0, 1.0};
        }

        const Vec3 base_dir = {
            frame.camera_position.x - frame.camera_target.x,
            frame.camera_position.y - frame.camera_target.y,
            frame.camera_position.z - frame.camera_target.z,
        };
        const double d = norm(base_dir);
        Vec3 dir = base_dir;
        if (d <= 1e-8) {
            dir = {-frame.satellite_tangent.x, -frame.satellite_tangent.y, -frame.satellite_tangent.z};
        }
        dir = normalize(dir);

        const double min_track_distance = std::max(0.35, frame.satellite_render_size * 12.0);
        const double desired_distance = std::max(min_track_distance, d * distance_scale_);
        pose_.eye = {
            frame.camera_target.x + dir.x * desired_distance,
            frame.camera_target.y + dir.y * desired_distance,
            frame.camera_target.z + dir.z * desired_distance,
        };
        pose_.target = frame.camera_target;

        Vec3 view_dir = normalize({
            pose_.target.x - pose_.eye.x,
            pose_.target.y - pose_.eye.y,
            pose_.target.z - pose_.eye.z,
        });

        // Keep camera roll stable by projecting "up" onto the image plane.
        const double ref_on_view = dot(up_ref, view_dir);
        Vec3 up = normalize({
            up_ref.x - view_dir.x * ref_on_view,
            up_ref.y - view_dir.y * ref_on_view,
            up_ref.z - view_dir.z * ref_on_view,
        });
        if (norm(up) <= 1e-8) {
            up = frame.orbital_normal;
            if (norm(up) <= 1e-12) {
                up = {0.0, 0.0, 1.0};
            }
        }

        if (has_last_up_ && dot(up, last_up_) < 0.0) {
            up = {-up.x, -up.y, -up.z};
        }
        if (has_last_up_) {
            constexpr double kUpBlend = 0.20;
            up = normalize({
                (1.0 - kUpBlend) * last_up_.x + kUpBlend * up.x,
                (1.0 - kUpBlend) * last_up_.y + kUpBlend * up.y,
                (1.0 - kUpBlend) * last_up_.z + kUpBlend * up.z,
            });
        }
        last_up_ = up;
        has_last_up_ = true;
        pose_.up = up;
    }

    CameraPose pose() const override {
        return pose_;
    }

    std::string debug_text() const override {
        std::ostringstream out;
        out << "track:zoom=" << distance_scale_;
        return out.str();
    }

private:
    CameraPose pose_{};
    double distance_scale_ = 1.0;
    Vec3 last_up_{0.0, 0.0, 1.0};
    bool has_last_up_ = false;
};

}  // namespace

CameraController::CameraController()
    : free_orbit_mode_(std::make_unique<FreeOrbitCameraMode>()),
      track_mode_(std::make_unique<TrackSatelliteCameraMode>()) {}

void CameraController::set_mode(CameraModeType mode) {
    active_mode_ = mode;
}

void CameraController::toggle_mode() {
    active_mode_ = (active_mode_ == CameraModeType::FreeOrbit)
                       ? CameraModeType::TrackSatellite
                       : CameraModeType::FreeOrbit;
}

CameraModeType CameraController::mode() const {
    return active_mode_;
}

void CameraController::on_input(const InputEvent& event) {
    if (active_mode_ == CameraModeType::TrackSatellite) {
        track_mode_->on_input(event);
    } else {
        free_orbit_mode_->on_input(event);
    }
}

void CameraController::update(const SimulationFrame& frame, double dt_s) {
    if (active_mode_ == CameraModeType::TrackSatellite) {
        track_mode_->update(frame, dt_s);
    } else {
        free_orbit_mode_->update(frame, dt_s);
    }
}

CameraPose CameraController::current_pose() const {
    return (active_mode_ == CameraModeType::TrackSatellite)
               ? track_mode_->pose()
               : free_orbit_mode_->pose();
}

std::string CameraController::debug_text() const {
    return (active_mode_ == CameraModeType::TrackSatellite)
               ? track_mode_->debug_text()
               : free_orbit_mode_->debug_text();
}

}  // namespace svl
