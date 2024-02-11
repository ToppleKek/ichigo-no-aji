#pragma once
#include "common.hpp"

template<typename T>
struct Vec2 {
    union {
        struct {
            T x;
            T y;
        };

        struct {
            T r;
            T g;
        };
    };
};

template<typename T>
struct Vec3 {
    union {
        struct {
            T x;
            T y;
            T z;
        };

        struct {
            T r;
            T g;
            T b;
        };
    };
};

template<typename T>
struct Vec4 {
    union {
        struct {
            T x;
            T y;
            T z;
            T w;
        };

        struct {
            T r;
            T g;
            T b;
            T a;
        };
    };
};

template<typename T>
struct Mat4x4 {
    T data[4][4];
};

inline Mat4x4<f32> orthogonal_projection_matrix(f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf) {
    Mat4x4<f32> ret;
    ret.data[0][0] = 2 / (r - l);
    ret.data[1][1] = 2 / (t - b);
    ret.data[2][2] = 2 / (zf - zn);
    ret.data[3][0] = -(r + l) / (r - l);
    ret.data[3][1] = -(t + b) / (t - b);
    ret.data[3][2] = - (zf + zn) / (zf - zn);
    return ret;
}
