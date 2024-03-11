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

    Vec2<T> &operator*=(T s) {
        x *= s;
        y *= s;
        return *this;
    }

    Vec2<T> &operator+=(const Vec2<T> &rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vec2<T> operator+(const Vec2<T> &rhs) {
        return { x + rhs.x, y + rhs.y };
    }

    Vec2<T> operator*(const T rhs) {
        return { x * rhs, y * rhs };
    }

    Vec2<T> operator-(const Vec2<T> &rhs) {
        return { x - rhs.x, y - rhs.y };
    }

    T length() {
        return sqrt(x * x + y * y);
    }
};

template<typename T>
Vec2<T> operator*(const T lhs, const Vec2<T> &rhs) {
    return { rhs.x * lhs, rhs.y * lhs };
}

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

    Vec3<T> &operator*=(T s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vec3<T> &operator+=(const Vec3<T> &rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vec3<T> operator+(const Vec3<T> &rhs) {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }

    Vec3<T> operator*(const T rhs) {
        return { x * rhs, y * rhs, z * rhs };
    }

    Vec3<T> operator-(const Vec3<T> &rhs) {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }


    T length() {
        return sqrt(x * x + y * y + z * z);
    }
};

template<typename T>
Vec3<T> operator*(const T lhs, const Vec3<T> &rhs) {
    return { rhs.x * lhs, rhs.y * lhs, rhs.z * lhs };
}

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

struct RectangleCollider {
    Vec2<f32> pos;
    f32 w;
    f32 h;
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

inline bool rectangles_intersect(RectangleCollider rect1, RectangleCollider rect2) {
    return ((rect1.pos.x > rect2.pos.x && rect1.pos.x < rect2.pos.x + rect2.w) || (rect2.pos.x > rect1.pos.x && rect2.pos.x < rect1.pos.x + rect1.w)) &&
           ((rect1.pos.y > rect2.pos.y - rect2.h && rect1.pos.y < rect2.pos.y) || (rect2.pos.y > rect1.pos.y - rect1.h && rect2.pos.y < rect1.pos.y));
}

template<typename T>
inline T dot(Vec2<T> lhs, Vec2<T> rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline f32 safe_ratio_1(f32 dividend, f32 divisor) {
    if (divisor == 0)
        return 1.0f;

    return dividend / divisor;
}
