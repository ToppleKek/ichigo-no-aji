#pragma once

#include "../bana_types.hpp"

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
    HOW_TO_PLAY,
    EXPLANATION_CONTROLLER,
    EXPLANATION_KEYBOARD,
    GOT_IT,
    CREDIT_TEXT,
    SAVE_GAME,
    HEALTH_UI,
    MONEY_UI,
    SHOP_HEADER,
    SHOPKEEP_WELCOME,
    SHOPKEEP_THANK_YOU,
    SHOPKEEP_YOU_ARE_BROKE,
    SHOPKEEP_SOLD_OUT,
    SHOP_SOLD_OUT,
    BUCHOU_DIALOGUE_1,
    BUCHOU_DIALOGUE_2,
    BUCHOU_DIALOGUE_3,
    BUCHOU_DIALOGUE_4,
    BUCHOU_DIALOGUE_5,
    FIRE_SPELL_EXPLAIN_1,
    FIRE_SPELL_EXPLAIN_2,
    STRING_COUNT
};

using StringID = i64;

static const constexpr char *STRINGS[STRING_COUNT][LANGUAGE_COUNT] = {
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
    {"How to play!", "やり方！"},
    {
        "Jump by pressing A or B.\n"
        "Run by holding X or Y.\n"
        "In midair, dive by pressing LB or RB.\n"
        "Upon landing from a dive, press A or B to jump out of the dive.",
        "AかBを押すとジャンプ出来ます。\n"
        "XかYを長押しすると、走ることが出来ます。\n"
        "空中でLBかRBを押すと、ダイブ出来ます。\n"
        "ダイブから地面に着地をした後、AかBを押すと、またジャンプ出来ます。"
    },
    {
        "Jump by pressing space.\n"
        "Run by holding left shift.\n"
        "In midair, dive by pressing Z.\n"
        "Upon landing from a dive, press space to jump out of the dive.",
        "SPACEを押すとジャンプ出来ます。\n"
        "LSHIFTを長押しすると、走ることが出来ます。\n"
        "空中でZを押すと、ダイブ出来ます。\n"
        "ダイブから地面に着地をした後、SPACEを押すと、またジャンプ出来ます。"
    },
    {"Got it!", "分かった！"},
    {
        "Credits:\n"
        "Player sprite: Rabi-Ribi (CreSpirit)\n"
        "BGM: Get On With It - 3R2\n"
        "Tile art: Joel Lucenay",
        "クレジット：\n"
        "プレイヤーのスプライト： ラビリビ (CreSpirit)\n"
        "BGM： Get On With It - 3R2"
        "\nタイルマップ： Joel Lucenay"
    },
    {"Save", "セーブ"},
    {"Health:", "Health:"},
    {"Money:", "お金:"},
    {"Shop", "コンビニ"},
    {"Text", "いらっしゃいませー"},
    {"Thanks", "まいどあり"},
    {"You can't afford that", "お金が足りないよ"},
    {"I'm all out of that", "売り切れたよ"},
    {"Sold out", "売り切れ"},
    {"Some story text 1", "部長のセリフ"},
    {"Some story text 2", "部長のセリフその2"},
    {
        "Some long story text 3 that has multiple line breaks in it and is really long so that we can test how the line\nspacing is."
        " Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore\net dolore magna aliqua."
        " Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip\nex ea commodo consequat.",
        "品質保証のためのセリフです！このセリフはすごく長いので、行間隔が見えます！こんにちは！\nいちごのあじ！無責任部！これは無責任部。俺は部長。よろしく。"
    },
    {"The club leader hands you a small key.", "部長に小さな鍵をもらった。"},
    {"Go on. The cave is right over there.", "ほら行け。洞窟はあそこだぞ"},
    {"You got the fire spell! Using this spell will shoot a fireball that can deal more damage and melt ice.", "ファイア魔法を手に入れました！この魔法で攻撃力が高くなり、氷を溶かせることになります。"},
    {"Press TAB or SELECT to switch spells.", "TABまたはSELECTを押すと魔法を交換できます。"},
};

extern Language current_language;

#define TL_STR(STR) (STRINGS[STR][current_language])

static constexpr StringID BUCHOU_DIALOGUE[] = {
    BUCHOU_DIALOGUE_1,
    BUCHOU_DIALOGUE_2,
    BUCHOU_DIALOGUE_3,
    BUCHOU_DIALOGUE_4,
};

static constexpr StringID BUCHOU_DIALOGUE_HAVE_KEY[] = {
    BUCHOU_DIALOGUE_5,
};

static constexpr StringID DIALOGUE_FIRE_SPELL_EXPLAIN[] = {
    FIRE_SPELL_EXPLAIN_1,
    FIRE_SPELL_EXPLAIN_2,
};
