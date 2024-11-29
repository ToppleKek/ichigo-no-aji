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
    PAUSE,
    RESUME_GAME,
    RETURN_TO_MENU,
    CONTROLLER_CONNECTED,
    CONTROLLER_DISCONNECTED,
    STRING_COUNT
};

static const char *STRINGS[STRING_COUNT][LANGUAGE_COUNT] = {
    {"Ichigo! Demo Game", "いちご！デモンストレーションゲーム"},
    {"Language: English", "言語：日本語"},
    {"Start game", "始める"},
    {"Exit", "終了"},
    {"F5: Change language - 2024 Braeden Hong - ichigo no chikara de...!", "F5: 言語変換 - 2024 Braeden Hong - 苺の力で...!"},
    {"Pause", "ポーズ"},
    {"Resume", "再開"},
    {"Return to menu", "メインメニューに戻る"},
    {"Controller connected", "コントローラーが接続されました"},
    {"Controller disconnected", "コントローラーの接続を解除されました"},
};
