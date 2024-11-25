#pragma once
#include "common.hpp"
#include <cmath>

template<typename T>
inline T clamp(T value, T min, T max);

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

    Vec2<T> operator*(const Vec2<T> rhs) {
        return { x * rhs.x, y * rhs.y };
    }

    Vec2<T> operator-(const Vec2<T> &rhs) {
        return { x - rhs.x, y - rhs.y };
    }

    T length() {
        return sqrt(x * x + y * y);
    }

    void clamp(T min, T max) {
        x = ::clamp(x, min, max);
        y = ::clamp(y, min, max);
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

    void clamp(T min, T max) {
        x = ::clamp(x, min, max);
        y = ::clamp(y, min, max);
        z = ::clamp(z, min, max);
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
bool operator==(const Vec2<T> &lhs, const Vec2<T> &rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

template<typename T>
bool operator==(const Vec3<T> &lhs, const Vec3<T> &rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

template<typename T>
bool operator==(const Vec4<T> &lhs, const Vec4<T> &rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

template<typename T>
bool operator!=(const Vec2<T> &lhs, const Vec2<T> &rhs) {
    return !(lhs == rhs);
}

template<typename T>
bool operator!=(const Vec3<T> &lhs, const Vec3<T> &rhs) {
    return !(lhs == rhs);
}

template<typename T>
bool operator!=(const Vec4<T> &lhs, const Vec4<T> &rhs) {
    return !(lhs == rhs);
}

template<typename T, typename U>
inline Vec2<T> vector_cast(const Vec2<U> &v) {
    return {
        (T) v.x,
        (T) v.y,
    };
}

template<typename T, typename U>
inline Vec3<T> vector_cast(const Vec3<U> &v) {
    return {
        (T) v.x,
        (T) v.y,
        (T) v.z,
    };
}

template<typename T, typename U>
inline Vec4<T> vector_cast(const Vec4<U> &v) {
    return {
        (T) v.x,
        (T) v.y,
        (T) v.z,
        (T) v.w,
    };
}

template<typename T>
struct Mat4 {
    Vec4<T> a;
    Vec4<T> b;
    Vec4<T> c;
    Vec4<T> d;


    Mat4<T> operator*(const Mat4<T> rhs) {
        return {
            {dot(a, {rhs.a.x, rhs.b.x, rhs.c.x, rhs.d.x}), dot(a, {rhs.a.y, rhs.b.y, rhs.c.y, rhs.d.y}), dot(a, {rhs.a.z, rhs.b.z, rhs.c.z, rhs.d.z}), dot(a, {rhs.a.w, rhs.b.w, rhs.c.w, rhs.d.w})},
            {dot(b, {rhs.a.x, rhs.b.x, rhs.c.x, rhs.d.x}), dot(b, {rhs.a.y, rhs.b.y, rhs.c.y, rhs.d.y}), dot(b, {rhs.a.z, rhs.b.z, rhs.c.z, rhs.d.z}), dot(b, {rhs.a.w, rhs.b.w, rhs.c.w, rhs.d.w})},
            {dot(c, {rhs.a.x, rhs.b.x, rhs.c.x, rhs.d.x}), dot(c, {rhs.a.y, rhs.b.y, rhs.c.y, rhs.d.y}), dot(c, {rhs.a.z, rhs.b.z, rhs.c.z, rhs.d.z}), dot(c, {rhs.a.w, rhs.b.w, rhs.c.w, rhs.d.w})},
            {dot(d, {rhs.a.x, rhs.b.x, rhs.c.x, rhs.d.x}), dot(d, {rhs.a.y, rhs.b.y, rhs.c.y, rhs.d.y}), dot(d, {rhs.a.z, rhs.b.z, rhs.c.z, rhs.d.z}), dot(d, {rhs.a.w, rhs.b.w, rhs.c.w, rhs.d.w})}
        };
    }
};

template <typename T>
struct Rect {
    Vec2<T> pos;
    T w;
    T h;
};

inline Mat4<f32> m4identity() {
    return {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
}

inline Mat4<f32> translate2d(Vec2<f32> t) {
    return {
        {1, 0, 0, t.x},
        {0, 1, 0, t.y},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
}

inline Mat4<f32> scale2d(Vec2<f32> s) {
    return {
        {s.x, 0,   0, 0},
        {0,   s.y, 0, 0},
        {0,   0,   1, 0},
        {0,   0,   0, 1},
    };
}

inline Vec2<f32> get_translation2d(Mat4<f32> m) {
    return {m.a.w, m.b.w};
}

// inline Mat4<f32> orthogonal_projection_matrix(f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf) {
//     Mat4<f32> ret;
//     ret.a.x = 2 / (r - l);
//     ret.b.y = 2 / (t - b);
//     ret.c.z = 2 / (zf - zn);
//     ret.d.x = -(r + l) / (r - l);
//     ret.d.y = -(t + b) / (t - b);
//     ret.d.z = - (zf + zn) / (zf - zn);
//     return ret;
// }

inline bool rectangles_intersect(Rect<f32> rect1, Rect<f32> rect2) {
    return ((rect1.pos.x > rect2.pos.x && rect1.pos.x < rect2.pos.x + rect2.w) || (rect2.pos.x > rect1.pos.x && rect2.pos.x < rect1.pos.x + rect1.w)) &&
           ((rect1.pos.y > rect2.pos.y && rect1.pos.y < rect2.pos.y + rect2.h) || (rect2.pos.y > rect1.pos.y && rect2.pos.y < rect1.pos.y + rect1.h));
}

inline f32 pixels_to_metres(f32 pixels) {
    return pixels / PIXELS_PER_METRE;
}

template<typename T>
inline T dot(Vec2<T> lhs, Vec2<T> rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

template<typename T>
inline T dot(Vec4<T> lhs, Vec4<T> rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

inline f32 safe_ratio_1(f32 dividend, f32 divisor) {
    if (divisor == 0)
        return 1.0f;

    return dividend / divisor;
}

inline f32 safe_ratio_0(f32 dividend, f32 divisor) {
    if (divisor == 0)
        return 0.0f;

    return dividend / divisor;
}

template<typename T>
inline T clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

inline f32 ichigo_lerp(f32 a, f32 t, f32 b) {
    return a + t * (b - a);
}

inline f32 signof(f32 value) {
    return value < 0.0f ? -1.0f : 1.0f;
}
