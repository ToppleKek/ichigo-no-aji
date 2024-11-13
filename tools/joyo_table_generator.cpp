/*
    Joyo and Jinmeiyo kanji array generator. Part of the Ichigo! project.

    Given a text file of utf8-encoded characters, creates a file called "kanji.bin" that is a u32 array
    of the corresponding unicode codepoints. Outputs Ichigo! C++ code for embeding the file, and defining
    extra metadata.

    Author: Braeden Hong
    Date:   2024/11/12

*/

#define _CRT_SECURE_NO_WARNINGS
#include "../common.hpp"

#include <cstdlib>

i32 main(i32 argc, char **argv) {
    if (argc != 2) {
        ICHIGO_ERROR("Usage: %s <kanji_file>", argv[0]);
        return 1;
    }

    std::FILE *kanji_file = std::fopen(argv[1], "rb");
    if (!kanji_file) {
        ICHIGO_ERROR("File not found.");
        return 1;
    }

    std::fseek(kanji_file, 0, SEEK_END);
    i64 file_size = std::ftell(kanji_file);
    std::fseek(kanji_file, 0, SEEK_SET);

    u8 *data = (u8 *) std::malloc(file_size);

    std::fread(data, 1, file_size, kanji_file);
    std::fclose(kanji_file);

    std::FILE *out_file = std::fopen("kanji.bin", "wb");

    i64 i = 0;
    u32 codepoint_count = 0;
    while (i < file_size) {
        u32 codepoint = 0;
        if (data[i] <= 0x7F) {
            codepoint = data[i];
            ++i;
        } else if ((data[i] & 0b11110000) == 0b11110000) {
            codepoint = ((data[i] & 0b00000111) << 18) | ((data[i + 1] & 0b00111111) << 12) | ((data[i + 2] & 0b00111111) << 6) | (data[i + 3] & 0b00111111);
            i += 4;
        } else if ((data[i] & 0b11100000) == 0b11100000) {
            codepoint = ((data[i] & 0b00001111) << 12) | ((data[i + 1] & 0b00111111) << 6) | ((data[i + 2] & 0b00111111));
            i += 3;
        } else if ((data[i] & 0b11000000) == 0b11000000) {
            codepoint = ((data[i] & 0b00011111) << 6) | ((data[i + 1] & 0b00111111));
            i += 2;
        } else {
            ICHIGO_ERROR("Not a valid utf8 character: %u", data[i]);
            return 1;
        }

        std::fwrite(&codepoint, sizeof(u32), 1, out_file);
        ++codepoint_count;
    }

    std::printf("EMBED(\"kanji.bin\", kt_bytes);\nstatic const u32 *kanji_codepoints = (u32 *) kt_bytes;\nstatic const u32 kanji_codepoint_count = %u;\n", codepoint_count);

    std::fclose(out_file);
    std::free(data);
    return 0;
}
