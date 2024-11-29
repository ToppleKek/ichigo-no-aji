#pragma once

namespace Ichiaji {
enum ProgramState {
    MAIN_MENU,
    GAME,
    PAUSE,
};

extern ProgramState program_state;
}