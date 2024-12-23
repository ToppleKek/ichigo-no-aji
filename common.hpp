/*
    Ichigo! A simple, from scratch, minimal dependency game engine for 2D side scrolling games.

    Commonly used types and macros.

    Author:      Braeden Hong
    Last edited: 2024/12/1
*/

#pragma once
#include <cstdint>
#include <cstdio>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using iptr  = std::intptr_t;
using uptr  = std::uintptr_t;
using usize = std::size_t;

using f32 = float;
using f64 = double;

#define MIN(A, B) (A < B ? A : B)
#define MAX(A, B) (A > B ? A : B)
#define SIGNOF(A) (A < 0 ? -1 : 1)
#define DISTANCE(A, B) (MAX(A, B) - (MIN(A, B)))
#define DEC_POSITIVE_OR(X, ALT) (X == 0 ? ALT : X - 1)
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define KILOBYTES(N) (N * 1024)
#define MEGABYTES(N) (N * KILOBYTES(1024))

#ifdef ICHIGO_DEBUG
#define ICHIGO_INFO(fmt, ...) std::printf("(info) %s:%d: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define ICHIGO_ERROR(fmt, ...) std::printf("(error) %s:%d: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define VK_ASSERT_OK(err) assert(err == VK_SUCCESS)
#else
#define ICHIGO_INFO(...)
#define ICHIGO_ERROR(...)
#define VK_ASSERT_OK(...)
#endif

#define SET_FLAG(FLAGS, FLAG)    (FLAGS |= FLAG)
#define CLEAR_FLAG(FLAGS, FLAG)  (FLAGS &= ~FLAG)
#define FLAG_IS_SET(FLAGS, FLAG) ((bool) (FLAGS & FLAG))

// Embed a file in the executable. Defines 3 variables:
// VNAME - an array of the file contents.
// VNAME_end - a pointer to the end of the file.
// VNAME_len - the length of the file.
#define EMBED(FNAME, VNAME)                                                               \
    __asm__(                                                                              \
        ".section .rodata    \n"                                                          \
        ".global " #VNAME "    \n.align 16\n" #VNAME ":    \n.incbin \"" FNAME            \
        "\"       \n"                                                                     \
        ".global " #VNAME "_end\n.align 1 \n" #VNAME                                      \
        "_end:\n.byte 1                   \n"                                             \
        ".global " #VNAME "_len\n.align 16\n" #VNAME "_len:\n.int " #VNAME "_end-" #VNAME \
        "\n"                                                                              \
        ".align 16           \n.text    \n");                                             \
    extern "C" {                                                                          \
    alignas(16) extern const unsigned char VNAME[];                                       \
    alignas(16) extern const unsigned char *const VNAME##_end;                            \
    extern const unsigned int VNAME##_len;                                                \
    }

// TODO: Make these configurable?
#define SCREEN_TILE_WIDTH 16
#define SCREEN_TILE_HEIGHT 9
#define PIXELS_PER_METRE 32
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNEL_COUNT 2

#ifdef _WIN32
#define platform_alloca _alloca
#endif
#ifdef unix
#define platform_alloca alloca
#endif
