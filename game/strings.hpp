#pragma once

enum Language {
    ENGLISH,
    JAPANESE,
    LANGUAGE_COUNT
};

enum Strings {
    TITLE,
    SWITCH_LANGUAGE,
    START_GAME,
    EXIT,
    INFO_TEXT,
    STRING_COUNT
};

static const char *STRINGS[STRING_COUNT][LANGUAGE_COUNT] = {
    {"Ichigo! Demo Game", "いちご！デモンストレーションゲーム"},
    {"Language: English", "言語：日本語"},
    {"Start game", "始める"},
    {"Exit", "終了"},
    {"F5: Change language - 2024 Braeden Hong - ichigo no chikara de...!", "F5: 言語変換 - 2024 Braeden Hong - 苺の力で...!"},
};
