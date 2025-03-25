/*
    A small collection of useful utility functions and classes.

    Author: Braeden Hong
      Date: October 30, 2023

    Last edited: 2025/03/24
*/

#pragma once

#include "common.hpp"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <stdarg.h>
#include "math.hpp"
#include "ichigo.hpp"

namespace Util {
/*
    A basic 'vector' implementation providing automatically expanding array storage.
*/
template<typename T>
class IchigoVector {
public:
    IchigoVector(u64 initial_capacity) : data(new T[initial_capacity]), capacity(initial_capacity) {}
    IchigoVector() = default;
    IchigoVector(const IchigoVector<T> &other) { operator=(other); }
    IchigoVector(IchigoVector<T> &&other) : size(other.size), data(other.data), capacity(other.capacity) { other.data = nullptr; other.capacity = 0; other.size = 0; }
    IchigoVector &operator=(const IchigoVector<T> &other) {
        capacity = other.capacity;
        size = other.size;

        if (other.data) {
            if (data)
                delete[] data;

            data = new T[capacity];

            for (u64 i = 0; i < size; ++i)
                data[i] = other.data[i];
        }

        return *this;
    }
    ~IchigoVector() { if (data) delete[] data; }

    T &at(u64 i)             { assert(i < size); return data[i]; }
    const T &at(u64 i) const { assert(i < size); return data[i]; }
    // Release the data managed by this vector. Useful for dynamically allocating an unknown amount of data and passing it along to library functions expecting a raw buffer.
    T *release_data()        { T *ret = data; data = nullptr; capacity = 0; size = 0; return ret; }
    void clear()             { size = 0; }

    void insert(u64 i, T item) {
        assert(i <= size);

        if (size == capacity)
            expand();

        if (i == size) {
            data[size++] = item;
            return;
        }

        std::memmove(&data[i + 1], &data[i], (size - i) * sizeof(T));
        data[i] = item;
        ++size;
    }

    u64 append(T item) {
        if (size == capacity)
            expand();

        data[size++] = item;
        return size - 1;
    }

    T remove(u64 i) {
        assert(i < size);

        if (i == size - 1)
            return data[--size];

        T ret = data[i];
        std::memmove(&data[i], &data[i + 1], (size - i - 1) * sizeof(T));
        --size;
        return ret;
    }

    i64 index_of(const T &item) const {
        for (i64 i = 0; i < size; ++i) {
            if (data[i] == item)
                return i;
        }

        return -1;
    }

    void resize(u64 size) {
        assert(size >= size);
        T *new_data = new T[size];

        for (u64 i = 0; i < size; ++i)
            new_data[i] = data[i];

        delete[] data;
        data = new_data;
        capacity = size;
    }

    u64 size = 0;
    T *data = nullptr;
private:
    u64 capacity = 0;

    void expand() {
        // Defer allocation to the first append call
        if (capacity == 0) {
            capacity = 16;
            data = new T[capacity];
            return;
        }

        T *new_data = new T[capacity *= 2];

        for (u64 i = 0; i < size; ++i)
            new_data[i] = data[i];

        delete[] data;
        data = new_data;
    }
};

class StringBuilder {
public:
    StringBuilder(u64 initial_capacity) : string(reinterpret_cast<char *>(std::malloc(initial_capacity))), capacity(initial_capacity) { string[0] = '\0'; }
    ~StringBuilder() { std::free(string); }

    void append(const char *string_to_append) {
        const u64 length = std::strlen(string_to_append);
        maybe_resize(length);

        size += length;
        std::strcat(string, string_to_append);
    }

    void append_json_serialized(const char *json_string) {
        const u64 length = std::strlen(json_string);
        maybe_resize(length);

        for (u64 i = 0; i < length; ++i) {
            if (json_string[i] == '\"') {
                string[size++] = '\\';
                string[size++] = json_string[i];
            } else if (json_string[i] == '\n') {
                string[size++] = '\\';
                string[size++] = 'n';
            } else if (json_string[i] == '\\') {
                string[size++] = '\\';
                string[size++] = '\\';
            } else {
                string[size++] = json_string[i];
            }
        }

        string[size] = '\0';
    }

    char *string;
    u64 size = 0;
private:
    u64 capacity;

    void maybe_resize(u64 space_needed) {
        if (size + space_needed + 1 >= capacity) {
            // TODO: Better allocation strategy?
            capacity += capacity + space_needed + 1;
            string = reinterpret_cast<char *>(std::realloc(string, capacity));
        }
    }
};

template<typename T>
struct IchigoCircularStack {
    IchigoCircularStack() = default;
    IchigoCircularStack(u64 capacity, T *memory) : top(0), bottom(0), count(0), capacity(capacity), data(memory) {}

    void push(T value) {
        data[top] = value;
        top       = (top + 1) % capacity;

        if (count != capacity) {
            ++count;
        } else {
            bottom = (bottom + 1) % capacity;
        }
    }

    T pop() {
        assert(count != 0);
        --count;

        if (top == 0) top = capacity - 1;
        else          --top;

        return data[top];
    }

    T peek() {
        assert(count != 0);
        return data[top - 1];
    }

    u64 top;
    u64 bottom;
    u64 count;
    u64 capacity;
    T *data;
};

inline Vec2<f32> calculate_centered_pos_offset(f32 col_width, f32 col_height, f32 sprite_width, f32 sprite_height) {
    return {(col_width - sprite_width) / 2.0f, col_height - sprite_height};
}

inline Bana::String tprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    auto ret = Bana::make_formatted_string(Ichigo::Internal::temp_allocator, fmt, args);

    va_end(args);
    return ret;
}

char *json_string_serialize(const char *json_string);
void json_return_serialized_string(char *json_string);
char *strcat_escape_quotes(char *dest, const char *source);
bool str_equal_case_insensitive(const char *lhs, const char *rhs);
u64 utf8_char_count(const char *utf8_string, usize utf8_string_length);
}
