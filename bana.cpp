#include "bana.hpp"
#include <stdarg.h>

Bana::Allocator Bana::heap_allocator = {
    .alloc = std::malloc,
    .free = std::free
};

static inline bool is_whitespace(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\f' || c == '\r' || c == '\v';
}

void *Bana::push_array(Arena *arena, usize size, usize count) {
    usize len = size * count;

    if (arena->pointer + len > arena->capacity) {
        assert(false && "Out of memory");
        return nullptr;
    }

    void *ret = &arena->data[arena->pointer];
    arena->pointer += len;
    return ret;
}

void *Bana::push_struct(Arena *arena, void *s, usize len) {
    if (arena->pointer + len > arena->capacity) {
        assert(false && "Out of memory");
        return nullptr;
    }

    void *ret = &arena->data[arena->pointer];
    std::memcpy(ret, s, len);
    arena->pointer += len;
    return ret;
}

Bana::String Bana::make_string(const char *cstr, Allocator allocator) {
    String str;

    str.length   = std::strlen(cstr);
    str.capacity = str.length;
    str.data     = (char *) allocator.alloc(str.capacity);

    std::memcpy(str.data, cstr, str.length);

    return str;
}

Bana::String Bana::make_string(const char *bytes, usize length, Allocator allocator) {
    String str;

    str.length   = length;
    str.capacity = str.length;
    str.data     = (char *) allocator.alloc(str.capacity);

    std::memcpy(str.data, bytes, str.length);

    return str;
}

Bana::String Bana::make_string(usize capacity, Allocator allocator) {
    String str;

    str.length   = 0;
    str.capacity = capacity;
    str.data     = (char *) allocator.alloc(str.capacity);

    return str;
}

Bana::String Bana::temp_string(const char *bytes, usize length) {
    String str;

    str.length   = length;
    str.capacity = str.length;
    str.data     = (char *) bytes;

    return str;
}

Bana::String Bana::temp_string(const char *cstr) {
    String str;

    str.length   = std::strlen(cstr);
    str.capacity = str.length;
    str.data     = (char *) cstr;

    return str;
}

void Bana::free_string(String *str, Allocator allocator) {
    allocator.free(str->data);
    std::memset(str, 0, sizeof(String));
}

void Bana::string_concat(String &dst, const String &src) {
    assert(dst.capacity >= dst.length + src.length);
    std::memcpy(&dst.data[dst.length], src.data, src.length);
    dst.length += src.length;
}

void Bana::string_concat(String &dst, char c) {
    assert(dst.capacity >= dst.length + 1);
    dst.data[dst.length] = c;
    ++dst.length;
}

void Bana::string_concat(String &dst, const char *cstr) {
    usize src_length = std::strlen(cstr);
    assert(dst.capacity >= dst.length + src_length);
    std::memcpy(&dst.data[dst.length], cstr, src_length);
    dst.length += src_length;
}

// FIXME: standard vsnprintf null terminates the string.
void Bana::string_format(String &dst, const char *fmt, ...) {
    dst.length = 0;

    va_list args;
    va_start(args, fmt);
    isize ret = std::vsnprintf(dst.data, dst.capacity, fmt, args);
    va_end(args);

    if (ret < 0 || ret > (isize) dst.capacity) return;

    dst.length = ret;
}

void Bana::string_strip_whitespace(String &str) {
    for (usize i = 0; i < str.length; ++i) {
        if (is_whitespace(str.data[i])) {
            std::memmove(&str.data[i], &str.data[i + 1], str.length - i - 1);
            --str.length;
            --i;
        }
    }
}

u32 Bana::parse_hex_u32(String str) {
    u32 res = 0;
    for (usize i = 0; i < str.length; ++i) {
        res *= 16;
        if      (str.data[i] >= 'A' && str.data[i] <= 'F') res += str.data[i] - 'A' + 10;
        else if (str.data[i] >= 'a' && str.data[i] <= 'f') res += str.data[i] - 'a' + 10;
        else if (str.data[i] >= '0' && str.data[i] <= '9') res += str.data[i] - '0';
        else return 0;
    }

    return res;
}
