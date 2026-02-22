#pragma once

#include "engine/types.hpp"

namespace svl {

Vec3 rotate_z(const Vec3& v, double angle_rad);
Vec3 cross(const Vec3& a, const Vec3& b);
double dot(const Vec3& a, const Vec3& b);
double norm(const Vec3& v);
Vec3 normalize(const Vec3& v);

}  // namespace svl

