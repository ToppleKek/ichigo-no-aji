set -e

OS="win32"
MODE="debug"

#-Wall -Wextra -Wpedantic -Wconversion
CXX_FLAGS="-std=c++20 -Wall -Wextra -fno-exceptions -Wno-deprecated-declarations -Wno-missing-braces"
CXX_FLAGS_GAME_DEBUG="-g -march=x86-64-v2 -DICHIGO_DEBUG"
CXX_FLAGS_GAME_RELEASE="-O3 -march=x86-64-v2 -Wl,/SUBSYSTEM:WINDOWS"
CXX_FLAGS_IMGUI="-O3"
CXX_FILES_WIN32="win32_ichigo.cpp"
CXX_FILES_LINUX="linux_ichigo.cpp"
CXX_FILES_ENGINE_DEBUG="main.cpp util.cpp entity.cpp camera.cpp asset.cpp mixer.cpp editor.cpp bana.cpp"
CXX_FILES_ENGINE_RELEASE="main.cpp util.cpp entity.cpp camera.cpp asset.cpp mixer.cpp bana.cpp"
CXX_FILES_GAME="game/ichiaji_main.cpp game/irisu.cpp"
IMGUI_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_win32.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
IMGUI_LINUX_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_sdl2.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
LIBS_WIN32="user32 -lwinmm -ldsound -ldxguid -lxinput -lgdi32"
LIBS_LINUX="GL -lSDL2"
EXE_NAME="game.exe"
IMGUI_OBJECT_FILES_DIRECTORY="build/imgui"
INCLUDE="thirdparty/include"

mkdir -p build

LIBS=""
CXX_FILES=""

if [ "$OS" = "linux" ]; then
    LIBS=$LIBS_LINUX
    if [ "$MODE" = "debug" ]; then
        CXX_FILES="$CXX_FILES_LINUX $CXX_FILES_ENGINE_DEBUG"
    elif [ "$MODE" = "release" ]; then
        CXX_FILES="$CXX_FILES_LINUX $CXX_FILES_ENGINE_RELEASE"
    else
        echo Invalid build mode
        exit 1
    fi
elif [ "$OS" = "win32" ]; then
    LIBS=$LIBS_WIN32
    if [ "$MODE" = "debug" ]; then
        CXX_FILES="$CXX_FILES_WIN32 $CXX_FILES_ENGINE_DEBUG"
    elif [ "$MODE" = "release" ]; then
        CXX_FILES="$CXX_FILES_WIN32 $CXX_FILES_ENGINE_RELEASE"
    else
        echo Invalid build mode
        exit 1
    fi
else
    echo Invalid platform
    exit 1
fi


if [ "${1}" = "run" ]; then
    cd build
    ./$EXE_NAME
    exit 0
fi

if [ "${1}" = "build" ]; then
    if [ "$OS" = "win32" ]; then
        ./thirdparty/tools/ctime.exe -begin ./build/timings.ctm
        if [ "$MODE" = "debug" ]; then
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -l ${LIBS} -I ${INCLUDE} ${CXX_FILES} ${CXX_FILES_GAME} ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o -o build/${EXE_NAME}
        elif [ "$MODE" = "release" ]; then
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -l ${LIBS} -I ${INCLUDE} ${CXX_FILES} ${CXX_FILES_GAME} -o build/${EXE_NAME}
        fi
        ./thirdparty/tools/ctime.exe -end ./build/timings.ctm
    else
        if [ "$MODE" = "debug" ]; then
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -l ${LIBS} -I ${INCLUDE} ${CXX_FILES} ${CXX_FILES_GAME} ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o -o build/${EXE_NAME}
        elif [ "$MODE" = "release" ]; then
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -l ${LIBS} -I ${INCLUDE} ${CXX_FILES} ${CXX_FILES_GAME} -o build/${EXE_NAME}
        fi
    fi
    exit 0
fi

if [ "${1}" = "shader" ]; then
    glslc shaders/main.frag -o build/frag.spv
    glslc shaders/main.vert -o build/vert.spv
    exit 0
fi

if [ "${1}" = "imgui" ]; then
    for file in ${IMGUI_CXX_FILES[*]}; do
        echo $file
        clang++ ${file} ${CXX_FLAGS} ${CXX_FLAGS_IMGUI} -I ${INCLUDE} -c -o ${IMGUI_OBJECT_FILES_DIRECTORY}/$(basename ${file}).o &
    done;

    wait $(jobs -p)
    exit 0
fi

echo fuck you
