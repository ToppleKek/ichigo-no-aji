/*
    Banalib
*/

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "bana_types.hpp"

#define MIN(A, B) (A < B ? A : B)
#define MAX(A, B) (A > B ? A : B)
#define SIGNOF(A) (A < 0 ? -1 : 1)
#define DISTANCE(A, B) (MAX(A, B) - (MIN(A, B)))
#define DEC_POSITIVE_OR(X, ALT) (X == 0 ? ALT : X - 1)
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define KILOBYTES(N) (N * 1024)
#define MEGABYTES(N) (N * KILOBYTES(1024))

#define PFBS(BANA_STRING) (i32) BANA_STRING.length, BANA_STRING.data
#define ICHIGO_INFO(fmt, ...) std::printf("(info) %s:%d: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define ICHIGO_ERROR(fmt, ...) std::printf("(error) %s:%d: " fmt "\n", __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__)
#define VK_ASSERT_OK(err) assert(err == VK_SUCCESS)

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

#ifdef _WIN32
#define platform_alloca _alloca
#endif
#ifdef unix
#define platform_alloca alloca
#endif

namespace Bana {
struct Arena {
    usize capacity;
    uptr  pointer;
    u8    *data;
};

#define BEGIN_TEMP_MEMORY(ARENA)        (ARENA.pointer)
#define END_TEMP_MEMORY(ARENA, POINTER) (ARENA.pointer = POINTER)
#define PUSH_STRUCT(ARENA, S)           Bana::push_struct(&ARENA, &S, sizeof(S))
#define PUSH_ARRAY(ARENA, TYPE, COUNT)  (TYPE *) Bana::push_array(&ARENA, sizeof(TYPE), COUNT)
#define BEGIN_LIST(ARENA, TYPE)         (TYPE *) (&ARENA.data[ARENA.pointer])
#define RESET_ARENA(ARENA)              ARENA.pointer = 0
void *push_array(Arena *arena, usize size, usize count);
void *push_struct(Arena *arena, void *s, usize len);

using AllocProc = void *(usize);
using FreeProc  = void (void *);
struct Allocator {
    AllocProc *alloc;
    FreeProc *free;
};

extern Allocator heap_allocator;

struct String {
    char *data;
    usize length;
    usize capacity;

    bool operator==(const Bana::String &rhs) {
        if (length != rhs.length) return false;
        return std::strncmp(data, rhs.data, length) == 0;
    }

    bool operator!=(const Bana::String &rhs) {
        return !(*this == rhs);
    }

    bool starts_with(const Bana::String &other) {
        if (length < other.length) return false;
        return std::strncmp(data, other.data, other.length) == 0;
    }
};

String make_string(const char *cstr, Allocator allocator = heap_allocator);
String make_string(const char *bytes, usize length, Allocator allocator = heap_allocator);
String make_string(usize capacity, Allocator allocator = heap_allocator);
void free_string(String *str, Allocator allocator = heap_allocator);
String temp_string(const char *bytes, usize length);
String temp_string(const char *cstr);
void string_concat(String &dst, String &src);
void string_concat(String &dst, char c);
void string_strip_whitespace(String &str);

#define MAKE_STACK_STRING(NAME, CAPACITY) Bana::String NAME = { (char *) platform_alloca(CAPACITY), 0, CAPACITY }

u32 parse_hex_u32(String str);

template<typename T>
struct Optional {
    bool has_value;
    T value;

    Optional(T v) {
        has_value = true;
        value     = v;
    }

    Optional() {
        has_value = false;
    }
};

template<typename T>
struct BufferBuilder {
    BufferBuilder(T *buffer, usize count) {
        capacity    = count;
        data        = buffer;
        size        = 0;
    }

    void append(T *items, usize count) {
        assert(size + count <= capacity);
        std::memcpy(&data[size], items, count * sizeof(T));
        size += count;
    }

    void append(T item) {
        append(&item, 1);
    }

    T *data;
    usize size;
    usize capacity;
};

template<typename T>
struct Array {
    isize size;
    isize capacity;
    T *data;

    Array(isize initial_capacity = 512) {
        size     = 0;
        capacity = initial_capacity;
        data     = (T *) std::malloc(capacity * sizeof(T));
    }

    ~Array() {
        std::free(data);
        data     = nullptr;
        size     = 0;
        capacity = 0;
    }

    isize append(T item) {
        if (size == capacity) expand();

        data[size++] = item;
        return size - 1;
    }

    T remove(isize i) {
        assert(i > 0 && i < size);
        if (i == size - 1) return data[--size];

        T ret = data[i];
        std::memmove(&data[i], &data[i + 1], (size - i - 1) * sizeof(T));
        --size;
        return ret;
    }

    void expand() {
        capacity *= 2;
        data = (T *) std::realloc(data, capacity * sizeof(T));
    }

    inline T &operator[](isize i) {
        assert(i < size);
        return data[i];
    }
};

template<typename T>
struct FixedArray {
    T *data;
    isize capacity;
    isize size;

    inline isize append(T item) {
        assert(size != capacity);
        data[size++] = item;
        return size - 1;
    }

    inline T &operator[](isize i) {
        assert(i < size);
        return data[i];
    }
};

template<typename Key, typename Value>
struct MapEntry {
    bool has_value;
    Key key;
    Value value;
};

template<typename Key, typename Value>
struct FixedMap {
    MapEntry<Key, Value> *data;
    isize capacity;
    isize size;

    inline void put(Key key, Value value) {
        assert(size != capacity);
        u32 sum = 0;
        for (u32 i = 0; i < sizeof(Key); ++i) {
            sum += ((u8 *) &key)[i];
        }

        isize hash = sum % capacity;
        for (isize j = hash;; j = (j + 1) % capacity) {
            if (!data[j].has_value) {
                data[j].has_value = true;
                data[j].key       = key;
                data[j].value     = value;
                ++size;
                break;
            }
        }
    }

    inline Bana::Optional<Value> get(Key key) {
        assert(size > 0);
        u32 sum = 0;
        for (u32 i = 0; i < sizeof(Key); ++i) {
            sum += ((u8 *) &key)[i];
        }

        isize hash = sum % capacity;
        for (isize i = hash, j = 0; j < capacity; i = (i + 1) % capacity, ++j) {
            if (data[i].has_value && std::memcmp(&data[i].key, &key, sizeof(Key)) == 0) {
                return data[i].value;
            }
        }

        return {};
    }
};

template<typename Key, typename Value>
FixedMap<Key, Value> make_fixed_map(isize capacity, Allocator allocator = heap_allocator) {
    FixedMap<Key, Value> map;

    map.size     = 0;
    map.capacity = capacity;
    map.data     = (MapEntry<Key, Value> *) allocator.alloc(capacity * sizeof(MapEntry<Key, Value>));

    return map;
}

template<typename Key, typename Value>
void free_fixed_map(FixedMap<Key, Value> *map, Allocator allocator = heap_allocator) {
    allocator.free(map->data);
    std::memset(map, 0, sizeof(FixedMap<Key, Value>));
}

template<typename T>
FixedArray<T> make_fixed_array(isize capacity, Allocator allocator = heap_allocator) {
    Bana::FixedArray<T> ret;

    ret.size     = 0;
    ret.capacity = capacity;
    ret.data     = (T *) allocator.alloc(capacity * sizeof(T));

    return ret;
}

template<typename T>
void free_fixed_array(FixedArray<T> *a, Allocator allocator = heap_allocator) {
    allocator.free(a->data);
    a->data     = nullptr;
    a->size     = 0;
    a->capacity = 0;
}

template<typename T>
void fixed_array_copy(FixedArray<T> &dst, FixedArray<T> &src) {
    assert(dst.capacity >= src.size);
    std::memcpy(dst.data, src.data, src.size * sizeof(T));
    dst.size = src.size;
}

#define MAKE_STACK_ARRAY(NAME, TYPE, CAPACITY) Bana::FixedArray<TYPE> NAME = { (TYPE *) platform_alloca(CAPACITY * sizeof(TYPE)), CAPACITY, 0 }
#define MAKE_GLOBAL_STATIC_ARRAY(NAME, TYPE, CAPACITY) static TYPE NAME##_DATA[CAPACITY]; Bana::FixedArray<TYPE> NAME = { NAME##_DATA, CAPACITY, 0 }

struct BufferReader {
    char *data;
    usize size;
    usize cursor;

    inline bool consume(char c) {
        if (data[cursor] != c) return false;

        ++cursor;
        return true;
    }

    inline char current() {
        return data[cursor];
    }

    inline char *current_ptr() {
        return &data[cursor];
    }

    inline u16 read16() {
        u16 value = *((u16 *) &data[cursor]);
        cursor += sizeof(u16);
        return value;
    }

    inline u32 read32() {
        u32 value = *((u32 *) &data[cursor]);
        cursor += sizeof(u32);
        return value;
    }

    inline char next() {
        if (cursor + 1 >= size) return '\0';

        return data[cursor + 1];
    }

    inline usize skip_to_after(char c) {
        usize bytes_skipped = 0;
        for (; cursor < size && data[cursor] != c; ++bytes_skipped, ++cursor);
        ++cursor;
        ++bytes_skipped;
        // FIXME: What do we do if we hit the end of the file?
        return bytes_skipped;
    }

    inline usize skip_to_next(char c) {
        usize bytes_skipped = 0;
        for (; cursor < size && data[cursor] != c; ++bytes_skipped, ++cursor);
        // FIXME: What do we do if we hit the end of the file?
        return bytes_skipped;
    }

    inline usize skip_to_after_sequence(Bana::String seq) {
        usize bytes_skipped = 0;
        for (; cursor < size; ++bytes_skipped, ++cursor) {
            if (std::strncmp(&data[cursor], seq.data, seq.length) == 0) break;
        }

        cursor += seq.length;
        bytes_skipped += seq.length;
        // FIXME: What do we do if we hit the end of the file?
        return bytes_skipped;
    }

    inline String slice_until(char c, Allocator allocator = heap_allocator) {
        char *data = current_ptr();
        usize size = skip_to_next(c);

        return make_string(data, size, allocator);
    }

    inline String view_until(char c) {
        char *data = current_ptr();
        usize size = skip_to_next(c);

        return temp_string(data, size);
    }

    inline bool has_more_data() {
        return cursor < size;
    }
};
}
