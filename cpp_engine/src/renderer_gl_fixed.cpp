#define NOMINMAX
#include <Windows.h>
#include <gdiplus.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "engine/renderer_gl_fixed.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "engine/ui.hpp"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

namespace svl {

namespace {

constexpr double kPi = 3.14159265358979323846;
GLuint g_earth_day_texture = 0U;
bool g_has_earth_day_texture = false;
ULONG_PTR g_gdiplus_token = 0;
bool g_gdiplus_ready = false;

bool initialize_gdiplus() {
    if (g_gdiplus_ready) {
        return true;
    }
    Gdiplus::GdiplusStartupInput startup_input;
    const Gdiplus::Status status =
        Gdiplus::GdiplusStartup(&g_gdiplus_token, &startup_input, nullptr);
    g_gdiplus_ready = (status == Gdiplus::Ok);
    return g_gdiplus_ready;
}

bool load_texture_rgba32_from_file(
    const std::filesystem::path& path,
    GLuint& out_texture_id) {
    if (path.empty() || !initialize_gdiplus()) {
        return false;
    }

    Gdiplus::Bitmap source(path.wstring().c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) {
        return false;
    }

    const UINT width = source.GetWidth();
    const UINT height = source.GetHeight();
    if (width == 0U || height == 0U) {
        return false;
    }

    Gdiplus::Bitmap converted(width, height, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(&converted);
        graphics.DrawImage(&source, 0, 0, width, height);
    }

    Gdiplus::Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    Gdiplus::BitmapData bmp_data{};
    if (converted.LockBits(
            &rect,
            Gdiplus::ImageLockModeRead,
            PixelFormat32bppARGB,
            &bmp_data) != Gdiplus::Ok) {
        return false;
    }

    const int stride = bmp_data.Stride;
    const size_t row_bytes = static_cast<size_t>(width) * 4U;
    std::vector<unsigned char> pixels(static_cast<size_t>(height) * row_bytes);
    const auto* src_base = static_cast<const unsigned char*>(bmp_data.Scan0);

    for (UINT y = 0; y < height; ++y) {
        const unsigned char* src_row = nullptr;
        if (stride >= 0) {
            src_row = src_base + static_cast<size_t>(y) * static_cast<size_t>(stride);
        } else {
            src_row = src_base + static_cast<size_t>(height - 1U - y) * static_cast<size_t>(-stride);
        }
        std::memcpy(
            pixels.data() + static_cast<size_t>(y) * row_bytes,
            src_row,
            row_bytes);
    }
    converted.UnlockBits(&bmp_data);

    GLuint texture_id = 0U;
    glGenTextures(1, &texture_id);
    if (texture_id == 0U) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (gluBuild2DMipmaps(
            GL_TEXTURE_2D,
            GL_RGBA,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            GL_BGRA,
            GL_UNSIGNED_BYTE,
            pixels.data()) != 0) {
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            GL_BGRA,
            GL_UNSIGNED_BYTE,
            pixels.data());
    }

    out_texture_id = texture_id;
    return true;
}

struct Rgba {
    double r;
    double g;
    double b;
    double a;
};

double smoothstep(double edge0, double edge1, double x) {
    if (edge1 <= edge0) {
        return (x < edge0) ? 0.0 : 1.0;
    }
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

Rgba earth_surface_rgba(const Vec3& n) {
    const double lat = std::asin(std::clamp(n.z, -1.0, 1.0));
    const double lon = std::atan2(n.y, n.x);

    const double n0 = std::sin(lon * 1.7 + std::sin(lat * 2.8) * 0.4);
    const double n1 = std::sin(lon * 3.9 - lat * 2.1);
    const double n2 = std::sin(lon * 8.3 + lat * 5.1);
    const double continent = 0.58 * n0 + 0.30 * n1 + 0.12 * n2;
    const bool is_land = continent > 0.08;

    const double dry = 0.5 + 0.5 * std::sin(lon * 5.0 - lat * 2.3);
    const double mountain = smoothstep(0.35, 0.75, continent);
    const double polar_ice = smoothstep(0.74, 0.96, std::abs(n.z));

    Rgba c{};
    if (is_land) {
        const double grass = 0.5 + 0.5 * std::sin(lon * 2.9 + lat * 3.4);
        const double land_r = 0.19 + 0.23 * dry + 0.10 * mountain;
        const double land_g = 0.30 + 0.35 * grass - 0.09 * dry + 0.06 * mountain;
        const double land_b = 0.11 + 0.10 * (1.0 - dry) + 0.05 * mountain;
        c = {land_r, land_g, land_b, 1.0};
    } else {
        const double depth = smoothstep(-0.95, 0.10, continent);
        c = {
            0.03 + 0.02 * depth,
            0.10 + 0.13 * depth,
            0.28 + 0.34 * depth,
            1.0,
        };
    }

    if (polar_ice > 0.0) {
        const double ice = polar_ice * 0.9;
        c.r = c.r * (1.0 - ice) + 0.90 * ice;
        c.g = c.g * (1.0 - ice) + 0.94 * ice;
        c.b = c.b * (1.0 - ice) + 0.98 * ice;
    }
    return c;
}

double cloud_alpha(const Vec3& n, double phase_rad) {
    const double lat = std::asin(std::clamp(n.z, -1.0, 1.0));
    const double lon = std::atan2(n.y, n.x);
    const double p0 = std::sin(5.4 * lon + 2.0 * lat + phase_rad * 0.9);
    const double p1 = std::sin(11.0 * lon - 4.6 * lat - phase_rad * 1.4);
    const double p2 = std::sin(21.0 * lon + 7.1 * lat + phase_rad * 0.6);
    const double v = 0.55 * p0 + 0.35 * p1 + 0.10 * p2;
    return 0.28 * smoothstep(0.12, 0.62, v);
}

void draw_box(double sx, double sy, double sz) {
    const double hx = sx * 0.5;
    const double hy = sy * 0.5;
    const double hz = sz * 0.5;
    glBegin(GL_QUADS);

    glNormal3d(0.0, 0.0, 1.0);
    glVertex3d(-hx, -hy, hz);
    glVertex3d(hx, -hy, hz);
    glVertex3d(hx, hy, hz);
    glVertex3d(-hx, hy, hz);

    glNormal3d(0.0, 0.0, -1.0);
    glVertex3d(-hx, -hy, -hz);
    glVertex3d(-hx, hy, -hz);
    glVertex3d(hx, hy, -hz);
    glVertex3d(hx, -hy, -hz);

    glNormal3d(1.0, 0.0, 0.0);
    glVertex3d(hx, -hy, -hz);
    glVertex3d(hx, hy, -hz);
    glVertex3d(hx, hy, hz);
    glVertex3d(hx, -hy, hz);

    glNormal3d(-1.0, 0.0, 0.0);
    glVertex3d(-hx, -hy, -hz);
    glVertex3d(-hx, -hy, hz);
    glVertex3d(-hx, hy, hz);
    glVertex3d(-hx, hy, -hz);

    glNormal3d(0.0, 1.0, 0.0);
    glVertex3d(-hx, hy, -hz);
    glVertex3d(-hx, hy, hz);
    glVertex3d(hx, hy, hz);
    glVertex3d(hx, hy, -hz);

    glNormal3d(0.0, -1.0, 0.0);
    glVertex3d(-hx, -hy, -hz);
    glVertex3d(hx, -hy, -hz);
    glVertex3d(hx, -hy, hz);
    glVertex3d(-hx, -hy, hz);

    glEnd();
}

void draw_sphere(double radius, int slices, int stacks) {
    for (int stack = 0; stack < stacks; ++stack) {
        const double v0 = static_cast<double>(stack) / stacks;
        const double v1 = static_cast<double>(stack + 1) / stacks;
        const double phi0 = (v0 - 0.5) * kPi;
        const double phi1 = (v1 - 0.5) * kPi;

        glBegin(GL_QUAD_STRIP);
        for (int slice = 0; slice <= slices; ++slice) {
            const double u = static_cast<double>(slice) / slices;
            const double theta = u * 2.0 * kPi;

            const double c0 = std::cos(phi0);
            const double s0 = std::sin(phi0);
            const double x0 = c0 * std::cos(theta);
            const double y0 = c0 * std::sin(theta);
            const double z0 = s0;

            const double c1 = std::cos(phi1);
            const double s1 = std::sin(phi1);
            const double x1 = c1 * std::cos(theta);
            const double y1 = c1 * std::sin(theta);
            const double z1 = s1;

            glNormal3d(x0, y0, z0);
            glVertex3d(x0 * radius, y0 * radius, z0 * radius);

            glNormal3d(x1, y1, z1);
            glVertex3d(x1 * radius, y1 * radius, z1 * radius);
        }
        glEnd();
    }
}

void draw_textured_sphere(double radius, int slices, int stacks) {
    for (int stack = 0; stack < stacks; ++stack) {
        const double v0 = static_cast<double>(stack) / stacks;
        const double v1 = static_cast<double>(stack + 1) / stacks;
        const double phi0 = (v0 - 0.5) * kPi;
        const double phi1 = (v1 - 0.5) * kPi;

        glBegin(GL_QUAD_STRIP);
        for (int slice = 0; slice <= slices; ++slice) {
            const double u = static_cast<double>(slice) / slices;
            const double theta = u * 2.0 * kPi;

            const double c0 = std::cos(phi0);
            const double s0 = std::sin(phi0);
            const Vec3 n0 = {c0 * std::cos(theta), c0 * std::sin(theta), s0};
            glTexCoord2d(u, 1.0 - v0);
            glNormal3d(n0.x, n0.y, n0.z);
            glVertex3d(n0.x * radius, n0.y * radius, n0.z * radius);

            const double c1 = std::cos(phi1);
            const double s1 = std::sin(phi1);
            const Vec3 n1 = {c1 * std::cos(theta), c1 * std::sin(theta), s1};
            glTexCoord2d(u, 1.0 - v1);
            glNormal3d(n1.x, n1.y, n1.z);
            glVertex3d(n1.x * radius, n1.y * radius, n1.z * radius);
        }
        glEnd();
    }
}

void draw_earth_surface(double radius, int slices, int stacks) {
    for (int stack = 0; stack < stacks; ++stack) {
        const double v0 = static_cast<double>(stack) / stacks;
        const double v1 = static_cast<double>(stack + 1) / stacks;
        const double phi0 = (v0 - 0.5) * kPi;
        const double phi1 = (v1 - 0.5) * kPi;

        glBegin(GL_QUAD_STRIP);
        for (int slice = 0; slice <= slices; ++slice) {
            const double u = static_cast<double>(slice) / slices;
            const double theta = u * 2.0 * kPi;

            const double c0 = std::cos(phi0);
            const double s0 = std::sin(phi0);
            const Vec3 n0 = {c0 * std::cos(theta), c0 * std::sin(theta), s0};
            const Rgba c_0 = earth_surface_rgba(n0);
            glColor4d(c_0.r, c_0.g, c_0.b, c_0.a);
            glNormal3d(n0.x, n0.y, n0.z);
            glVertex3d(n0.x * radius, n0.y * radius, n0.z * radius);

            const double c1 = std::cos(phi1);
            const double s1 = std::sin(phi1);
            const Vec3 n1 = {c1 * std::cos(theta), c1 * std::sin(theta), s1};
            const Rgba c_1 = earth_surface_rgba(n1);
            glColor4d(c_1.r, c_1.g, c_1.b, c_1.a);
            glNormal3d(n1.x, n1.y, n1.z);
            glVertex3d(n1.x * radius, n1.y * radius, n1.z * radius);
        }
        glEnd();
    }
}

void draw_earth_clouds(double radius, int slices, int stacks, double phase_rad) {
    for (int stack = 0; stack < stacks; ++stack) {
        const double v0 = static_cast<double>(stack) / stacks;
        const double v1 = static_cast<double>(stack + 1) / stacks;
        const double phi0 = (v0 - 0.5) * kPi;
        const double phi1 = (v1 - 0.5) * kPi;

        glBegin(GL_QUAD_STRIP);
        for (int slice = 0; slice <= slices; ++slice) {
            const double u = static_cast<double>(slice) / slices;
            const double theta = u * 2.0 * kPi;

            const double c0 = std::cos(phi0);
            const double s0 = std::sin(phi0);
            const Vec3 n0 = {c0 * std::cos(theta), c0 * std::sin(theta), s0};
            glColor4d(0.93, 0.96, 1.0, cloud_alpha(n0, phase_rad));
            glNormal3d(n0.x, n0.y, n0.z);
            glVertex3d(n0.x * radius, n0.y * radius, n0.z * radius);

            const double c1 = std::cos(phi1);
            const double s1 = std::sin(phi1);
            const Vec3 n1 = {c1 * std::cos(theta), c1 * std::sin(theta), s1};
            glColor4d(0.93, 0.96, 1.0, cloud_alpha(n1, phase_rad));
            glNormal3d(n1.x, n1.y, n1.z);
            glVertex3d(n1.x * radius, n1.y * radius, n1.z * radius);
        }
        glEnd();
    }
}

void draw_satellite_model(double size) {
    const double bus_x = size * 0.92;
    const double bus_y = size * 1.22;
    const double bus_z = size * 0.84;
    const double panel_x = size * 1.95;
    const double panel_y = size * 0.08;
    const double panel_z = size * 0.92;
    const double panel_offset = (bus_x * 0.5) + (panel_x * 0.5) + size * 0.08;

    glColor3d(0.72, 0.74, 0.77);
    draw_box(bus_x, bus_y, bus_z);

    glColor3d(0.08, 0.14, 0.30);
    glPushMatrix();
    glTranslated(panel_offset, 0.0, 0.0);
    draw_box(panel_x, panel_y, panel_z);
    glPopMatrix();
    glPushMatrix();
    glTranslated(-panel_offset, 0.0, 0.0);
    draw_box(panel_x, panel_y, panel_z);
    glPopMatrix();

    glColor3d(0.80, 0.77, 0.62);
    glPushMatrix();
    glTranslated(0.0, bus_y * 0.62, 0.0);
    draw_box(size * 0.08, size * 0.55, size * 0.08);
    glPopMatrix();

    glColor3d(0.66, 0.68, 0.72);
    glPushMatrix();
    glTranslated(0.0, bus_y * 0.95, 0.0);
    draw_sphere(size * 0.22, 16, 10);
    glPopMatrix();
}

void draw_circle_xy(double radius, int segments) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const double t = (static_cast<double>(i) / static_cast<double>(segments)) * 2.0 * kPi;
        glVertex3d(radius * std::cos(t), radius * std::sin(t), 0.0);
    }
    glEnd();
}

void draw_orbit(
    double orbit_radius,
    const Vec3& radial_hint,
    const Vec3& normal_hint,
    const Vec3& center) {
    const auto vec_norm = [](const Vec3& v) -> double {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    };
    const auto vec_normalize = [&](const Vec3& v) -> Vec3 {
        const double n = vec_norm(v);
        if (n <= 1e-10) {
            return {0.0, 0.0, 1.0};
        }
        return {v.x / n, v.y / n, v.z / n};
    };
    const auto vec_cross = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    };

    Vec3 n = vec_normalize(normal_hint);
    Vec3 u = vec_normalize(radial_hint);
    Vec3 v = vec_cross(n, u);
    if (vec_norm(v) <= 1e-10) {
        const Vec3 fallback = (std::abs(n.z) < 0.95) ? Vec3{0.0, 0.0, 1.0} : Vec3{0.0, 1.0, 0.0};
        u = vec_normalize(vec_cross(fallback, n));
        v = vec_normalize(vec_cross(n, u));
    } else {
        v = vec_normalize(v);
        u = vec_normalize(vec_cross(v, n));
    }

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 240; ++i) {
        const double t = (static_cast<double>(i) / 240.0) * 2.0 * kPi;
        const double ct = std::cos(t);
        const double st = std::sin(t);
        const Vec3 p = {
            orbit_radius * (u.x * ct + v.x * st),
            orbit_radius * (u.y * ct + v.y * st),
            orbit_radius * (u.z * ct + v.z * st),
        };
        glVertex3d(center.x + p.x, center.y + p.y, center.z + p.z);
    }
    glEnd();
}

void set_sun_direction(const Vec3& dir) {
    const GLfloat light_pos[4] = {
        static_cast<GLfloat>(dir.x),
        static_cast<GLfloat>(dir.y),
        static_cast<GLfloat>(dir.z),
        0.0F,
    };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
}

void set_sun_lighting(double sun_visibility, double ambient_floor, double exposure_scale) {
    const double vis = std::clamp(sun_visibility, 0.0, 1.0);
    const double exposure = std::max(0.0, exposure_scale);
    const double ambient_weight = std::clamp(
        ambient_floor + (1.0 - ambient_floor) * vis,
        0.0,
        1.0);

    const GLfloat ambient[4] = {
        static_cast<GLfloat>(0.020 * ambient_weight * exposure),
        static_cast<GLfloat>(0.020 * ambient_weight * exposure),
        static_cast<GLfloat>(0.026 * ambient_weight * exposure),
        1.0F,
    };
    const GLfloat diffuse[4] = {
        static_cast<GLfloat>(0.95 * vis * exposure),
        static_cast<GLfloat>(0.95 * vis * exposure),
        static_cast<GLfloat>(0.92 * vis * exposure),
        1.0F,
    };
    const GLfloat specular[4] = {
        static_cast<GLfloat>(0.20 * vis * exposure),
        static_cast<GLfloat>(0.20 * vis * exposure),
        static_cast<GLfloat>(0.20 * vis * exposure),
        1.0F,
    };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
}

}  // namespace

void initialize_gl_state(const ScenarioConfig& scenario) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.01F, 0.015F, 0.035F, 1.0F);
    const GLfloat global_ambient[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);
    set_sun_lighting(1.0, 0.04, 1.0);

    g_has_earth_day_texture = false;
    if (!scenario.earth_texture_day.empty()) {
        if (load_texture_rgba32_from_file(scenario.earth_texture_day, g_earth_day_texture)) {
            g_has_earth_day_texture = true;
            std::cout << "[cpp-engine] loaded earth day texture: "
                      << scenario.earth_texture_day.string() << "\n";
        } else {
            std::cerr << "[cpp-engine] warning: failed to load earth day texture: "
                      << scenario.earth_texture_day.string() << "\n";
        }
    }
}

std::vector<Vec3> build_star_field(int count, double radius) {
    std::vector<Vec3> stars;
    stars.reserve(static_cast<size_t>(count));
    constexpr double golden = 2.399963229728653;
    for (int i = 0; i < count; ++i) {
        const double t = (static_cast<double>(i) + 0.5) / static_cast<double>(count);
        const double y = 1.0 - 2.0 * t;
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double phi = golden * static_cast<double>(i);
        stars.push_back({std::cos(phi) * r * radius, y * radius, std::sin(phi) * r * radius});
    }
    return stars;
}

void render_scene(
    const RuntimeState& runtime,
    const SimulationFrame& frame,
    const CameraPose& camera_pose,
    const std::vector<Vec3>& stars,
    CameraModeType camera_mode) {
    const auto vec_norm = [](const Vec3& v) -> double {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    };
    const Vec3 eye_to_target = {
        camera_pose.eye.x - camera_pose.target.x,
        camera_pose.eye.y - camera_pose.target.y,
        camera_pose.eye.z - camera_pose.target.z,
    };
    const double view_distance = vec_norm(eye_to_target);
    const double scene_radius = std::max(
        std::max(frame.earth_render_radius, frame.orbit_render_radius),
        std::max(vec_norm(frame.earth_position), vec_norm(frame.satellite_position)));
    const double z_near = std::max(0.01, std::min(1.0, view_distance * 0.005));
    const double z_far = std::max(400.0, view_distance + scene_radius * 8.0 + 100.0);

    const double aspect = (runtime.viewport_height > 0)
                              ? static_cast<double>(runtime.viewport_width) / static_cast<double>(runtime.viewport_height)
                              : (16.0 / 9.0);
    glViewport(0, 0, runtime.viewport_width, runtime.viewport_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(frame.fov_y_deg, aspect, z_near, z_far);
    glMatrixMode(GL_MODELVIEW);

    glPolygonMode(GL_FRONT_AND_BACK, runtime.wireframe ? GL_LINE : GL_FILL);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    gluLookAt(
        camera_pose.eye.x,
        camera_pose.eye.y,
        camera_pose.eye.z,
        camera_pose.target.x,
        camera_pose.target.y,
        camera_pose.target.z,
        camera_pose.up.x,
        camera_pose.up.y,
        camera_pose.up.z);

    glDisable(GL_LIGHTING);
    glPointSize(2.0F);
    glColor3d(0.85, 0.88, 1.0);
    glBegin(GL_POINTS);
    for (const Vec3& s : stars) {
        glVertex3d(s.x, s.y, s.z);
    }
    glEnd();

    if (runtime.show_orbit) {
        glColor3d(0.33, 0.40, 0.56);
        draw_orbit(
            frame.orbit_render_radius,
            frame.satellite_radial,
            frame.orbital_normal,
            frame.earth_position);

        if (runtime.show_debug_guides) {
            glDisable(GL_LIGHTING);
            glColor3d(0.92, 0.35, 0.92);
            glBegin(GL_LINES);
            glVertex3d(
                frame.earth_position.x - frame.orbital_normal.x * frame.earth_render_radius * 1.35,
                frame.earth_position.y - frame.orbital_normal.y * frame.earth_render_radius * 1.35,
                frame.earth_position.z - frame.orbital_normal.z * frame.earth_render_radius * 1.35);
            glVertex3d(
                frame.earth_position.x + frame.orbital_normal.x * frame.earth_render_radius * 1.35,
                frame.earth_position.y + frame.orbital_normal.y * frame.earth_render_radius * 1.35,
                frame.earth_position.z + frame.orbital_normal.z * frame.earth_render_radius * 1.35);
            glEnd();
        }
    }

    glEnable(GL_LIGHTING);
    set_sun_direction(frame.sun_direction);
    set_sun_lighting(frame.sun_visibility, frame.ambient_floor, frame.exposure_scale);

    const GLfloat earth_specular[4] = {0.12F, 0.14F, 0.18F, 1.0F};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, earth_specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 22.0F);
    glPushMatrix();
    glTranslated(frame.earth_position.x, frame.earth_position.y, frame.earth_position.z);
    glRotatef(static_cast<GLfloat>(frame.earth_axial_tilt_deg), 1.0F, 0.0F, 0.0F);
    glRotatef(static_cast<GLfloat>(frame.earth_spin_deg), 0.0F, 0.0F, 1.0F);
    if (g_has_earth_day_texture) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_earth_day_texture);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor4d(1.0, 1.0, 1.0, 1.0);
        draw_textured_sphere(frame.earth_render_radius, 72, 72);
        glBindTexture(GL_TEXTURE_2D, 0U);
        glDisable(GL_TEXTURE_2D);
    } else {
        draw_earth_surface(frame.earth_render_radius, 72, 72);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    draw_earth_clouds(
        frame.earth_render_radius * 1.013,
        56,
        56,
        frame.earth_spin_deg * (kPi / 180.0) * 1.20);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4d(0.34, 0.56, 0.98, 0.07);
    draw_sphere(frame.earth_render_radius * 1.028, 48, 36);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    if (runtime.show_debug_guides) {
        glDisable(GL_LIGHTING);
        glColor3d(0.32, 0.95, 0.95);
        draw_circle_xy(frame.earth_render_radius * 1.004, 180);
        glBegin(GL_LINES);
        glVertex3d(0.0, 0.0, -frame.earth_render_radius * 1.35);
        glVertex3d(0.0, 0.0, frame.earth_render_radius * 1.35);
        glEnd();
        glEnable(GL_LIGHTING);
    }
    glPopMatrix();

    const GLfloat sat_specular[4] = {0.65F, 0.66F, 0.69F, 1.0F};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, sat_specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 64.0F);
    glPushMatrix();
    glTranslated(frame.satellite_position.x, frame.satellite_position.y, frame.satellite_position.z);
    glRotated(std::fmod(frame.theta * (180.0 / kPi) * 5.0, 360.0), 0.2, 0.7, 0.5);
    draw_satellite_model(frame.satellite_render_size);
    glPopMatrix();

    if (frame.show_sun_marker) {
        glDisable(GL_LIGHTING);
        glColor3d(0.95, 0.77, 0.20);
        const double sun_distance = std::max(5.8, frame.earth_render_radius * 3.0);
        glPushMatrix();
        glTranslated(
            frame.sun_direction.x * sun_distance,
            frame.sun_direction.y * sun_distance,
            frame.sun_direction.z * sun_distance);
        draw_sphere(std::max(0.09, frame.earth_render_radius * 0.03), 16, 12);
        glPopMatrix();
    } else {
        glDisable(GL_LIGHTING);
    }

    draw_camera_toggle_ui(
        runtime.viewport_width,
        runtime.viewport_height,
        camera_mode == CameraModeType::TrackSatellite);
}

}  // namespace svl
