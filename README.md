# Space Vision Engine (C++ Portfolio v0.1)

Real-time Win32 + OpenGL prototype for Earth-orbit visualization and camera/perception iteration.

This public portfolio snapshot includes:
- Fixed-step orbital simulation with smooth render interpolation
- Earth, satellite, orbit path, star field, and directional Sun lighting
- Analytic eclipse visibility scaling (umbra/penumbra approximation)
- Free camera and satellite-track camera modes
- JSON scenario configuration and runtime controls

This repository intentionally excludes private research/mining pipeline code and Blender tooling.

## Platform

- Windows (x64)
- CMake >= 3.20
- MSVC toolchain (Visual Studio Build Tools)
- System OpenGL (`opengl32`, `glu32`)

## Build

```powershell
cmake -S . -B build-cpp-min
cmake --build build-cpp-min --config Release --target space_vision_engine
```

## Run

```powershell
.\build-cpp-min\cpp_engine\Release\space_vision_engine.exe --config .\configs\scenarios\rendezvous_glint_fast_iter.json --speed 80 --fps 60
```

or with helper script:

```powershell
.\run_cpp_engine.ps1 -Config .\configs\scenarios\rendezvous_glint_fast_iter.json -Speed 80 -Fps 60
```

## Controls

- `Space`: pause/resume simulation
- `W`: wireframe toggle
- `O`: orbit line toggle
- `G`: debug guides toggle
- `Up` / `Down`: speed up / slow down
- `A` / `D`: yaw
- `R` / `F`: pitch
- `Z` / `X`: zoom
- `MMB + drag`: rotate camera
- `Mouse Wheel`: zoom
- `C`: toggle free / track camera
- `LMB` camera icon: toggle track mode
- `Esc`: quit

## Project Layout

- `cpp_engine/src/main.cpp`: app loop + composition
- `cpp_engine/src/simulation.cpp`: dynamics + frame construction
- `cpp_engine/src/renderer_gl_fixed.cpp`: rendering
- `cpp_engine/src/camera.cpp`: free/track camera modes
- `cpp_engine/src/config.cpp`: JSON + CLI parsing
- `cpp_engine/src/win32_window.cpp`: Win32 window/event layer

## License

MIT (see `LICENSE`).
