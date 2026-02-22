#include "engine/math.hpp"

#include <cmath>

namespace svl {

Vec3 rotate_z(const Vec3& v, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return {c * v.x - s * v.y, s * v.x + c * v.y, v.z};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double norm(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(const Vec3& v) {
    const double n = norm(v);
    if (n <= 1e-12) {
        return {0.0, 0.0, 1.0};
    }
    return {v.x / n, v.y / n, v.z / n};
}

}  // namespace svl

