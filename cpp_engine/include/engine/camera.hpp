#pragma once

#include <memory>
#include <string>

#include "engine/types.hpp"

namespace svl {

class ICameraMode {
public:
    virtual ~ICameraMode() = default;
    virtual void on_input(const InputEvent& event) = 0;
    virtual void update(const SimulationFrame& frame, double dt_s) = 0;
    virtual CameraPose pose() const = 0;
    virtual std::string debug_text() const = 0;
};

class CameraController {
public:
    CameraController();

    void set_mode(CameraModeType mode);
    void toggle_mode();
    CameraModeType mode() const;

    void on_input(const InputEvent& event);
    void update(const SimulationFrame& frame, double dt_s);

    CameraPose current_pose() const;
    std::string debug_text() const;

private:
    std::unique_ptr<ICameraMode> free_orbit_mode_;
    std::unique_ptr<ICameraMode> track_mode_;
    CameraModeType active_mode_ = CameraModeType::FreeOrbit;
};

}  // namespace svl

