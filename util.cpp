#include "util.hpp"
#include <cctype>

void *Util::push_array(Arena *arena, usize size, usize count) {
    usize len = size * count;

    if (arena->pointer + len > arena->capacity) {
        assert(false && "Out of memory");
        return nullptr;
    }

    void *ret = &arena->data[arena->pointer];
    arena->pointer += len;
    return ret;
}

void *Util::push_struct(Util::Arena *arena, void *s, usize len) {
    if (arena->pointer + len > arena->capacity) {
        assert(false && "Out of memory");
        return nullptr;
    }

    void *ret = &arena->data[arena->pointer];
    memcpy(ret, s, len);
    arena->pointer += len;
    return ret;
}

char *Util::json_string_serialize(const char *json_string) {
    const u32 length = std::strlen(json_string);
    IchigoVector<char> data(length);
    for (u32 i = 0; i < length; ++i) {
        if (json_string[i] == '"') {
            data.append('\\');
            data.append('\"');
        } else {
            data.append(json_string[i]);
        }
    }

    return data.release_data();
}

void Util::json_return_serialized_string(char *json_string) {
    delete[] json_string;
}

char *Util::strcat_escape_quotes(char *dest, const char *source) {
    for (; *dest != '\0'; ++dest);

    for (; *source != '\0'; ++source, ++dest) {
        if (*source == '"') {
            *dest++ = '\\';
        } else if (*source == '\\') {
            *dest++ = '\\';
        }

        *dest = *source;
    }

    *dest = '\0';
    return dest;
}

bool Util::str_equal_case_insensitive(const char *lhs, const char *rhs) {
    for (;; ++lhs, ++rhs) {
        if (*lhs == '\0' || *rhs == '\0')
            return *lhs == '\0' && *rhs == '\0';

        if (std::tolower(*lhs) != std::tolower(*rhs))
            return false;
    }

    return true;
}

u64 Util::utf8_char_count(const char *utf8_string, usize utf8_string_length) {
    u64 char_count = 0;

    for (usize i = 0; i < utf8_string_length; ++i) {
        if ((u8) utf8_string[i] >> 6 != 0b10)
            ++char_count;
    }

    return char_count;
}
