// CraftEngine - minimal row-vector math (matches HLSL `row_major` + mul(v, M))
#pragma once
#include <math.h>

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return (l > 1e-6f) ? v * (1.0f / l) : Vec3{0, 0, 0};
}

struct Mat4 {
    float m[4][4]{};

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    // Left-handed perspective, depth range [0,1] (D3D convention).
    static Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
        Mat4 r;
        float f = 1.0f / std::tan(fovYRadians * 0.5f);
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = zFar / (zFar - zNear);
        r.m[2][3] = 1.0f;
        r.m[3][2] = -(zNear * zFar) / (zFar - zNear);
        return r;
    }

    // Left-handed look-at for row vectors.
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 zAxis = normalize(target - eye);
        Vec3 xAxis = normalize(cross(up, zAxis));
        Vec3 yAxis = cross(zAxis, xAxis);

        Mat4 r;
        r.m[0][0] = xAxis.x; r.m[0][1] = yAxis.x; r.m[0][2] = zAxis.x; r.m[0][3] = 0;
        r.m[1][0] = xAxis.y; r.m[1][1] = yAxis.y; r.m[1][2] = zAxis.y; r.m[1][3] = 0;
        r.m[2][0] = xAxis.z; r.m[2][1] = yAxis.z; r.m[2][2] = zAxis.z; r.m[2][3] = 0;
        r.m[3][0] = -dot(xAxis, eye);
        r.m[3][1] = -dot(yAxis, eye);
        r.m[3][2] = -dot(zAxis, eye);
        r.m[3][3] = 1;
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}
