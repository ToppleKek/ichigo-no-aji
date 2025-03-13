set -e

OS=${2}
MODE=${3}
THREADS=${4}

#-Wall -Wextra -Wpedantic -Wconversion
CXX_FLAGS="-std=c++20 -Wall -Wextra -fno-exceptions -Wno-deprecated-declarations -Wno-missing-braces"
CXX_FLAGS_GAME_DEBUG="-g -march=x86-64-v2 -DICHIGO_DEBUG"
CXX_FLAGS_GAME_RELEASE="-O3 -march=x86-64-v2"
CXX_FLAGS_IMGUI="-O3"
CXX_FILES_WIN32=(win32_ichigo.cpp)
CXX_FILES_LINUX=(linux_ichigo.cpp)
CXX_FILES_ENGINE_DEBUG=(main.cpp util.cpp entity.cpp camera.cpp asset.cpp mixer.cpp editor.cpp bana.cpp)
CXX_FILES_ENGINE_RELEASE=(main.cpp util.cpp entity.cpp camera.cpp asset.cpp mixer.cpp bana.cpp)

CXX_FILES_GAME=(
    game/ichiaji_main.cpp
    game/irisu.cpp
    game/moving_platform.cpp
    game/particle_source.cpp
    game/entrances.cpp
    game/rabbit_enemy.cpp
    game/ui.cpp
    game/asset_catalog.cpp
)

IMGUI_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_win32.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
IMGUI_LINUX_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_sdl2.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
LIBS_WIN32="user32 -lwinmm -ldsound -ldxguid -lxinput -lgdi32"
LIBS_LINUX="GL -lSDL2"
EXE_NAME="game.exe"
IMGUI_OBJECT_FILES_DIRECTORY="build/imgui"
INCLUDE="thirdparty/include"

mkdir -p build/objects
mkdir -p $IMGUI_OBJECT_FILES_DIRECTORY

LIBS=""
CXX_FILES=("${CXX_FILES_GAME[@]}")

if [ "$OS" = "linux" ]; then
    LIBS=$LIBS_LINUX
    if [ "$MODE" = "debug" ]; then
        CXX_FILES+=("${CXX_FILES_LINUX[@]}" "${CXX_FILES_ENGINE_DEBUG[@]}")
    elif [ "$MODE" = "release" ]; then
        CXX_FILES+=("${CXX_FILES_LINUX[@]}" "${CXX_FILES_ENGINE_RELEASE[@]}")
    else
        echo Invalid build mode
        exit 1
    fi
elif [ "$OS" = "win32" ]; then
    LIBS=$LIBS_WIN32
    if [ "$MODE" = "debug" ]; then
        CXX_FILES+=("${CXX_FILES_WIN32[@]}" "${CXX_FILES_ENGINE_DEBUG[@]}")
    elif [ "$MODE" = "release" ]; then
        CXX_FILES+=("${CXX_FILES_WIN32[@]}" "${CXX_FILES_ENGINE_RELEASE[@]}")
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
    rm -f build/objects/*.o
    if [ "$OS" = "win32" ]; then
        type ./thirdparty/tools/ctime.exe && ./thirdparty/tools/ctime.exe -begin ./build/timings.ctm
        if [ "$MODE" = "debug" ]; then
            files_in_flight=0
            for file in ${CXX_FILES[*]}; do
                echo $file
                clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -I ${INCLUDE} ${file} -c -o build/objects/$(basename ${file}).o &
                (( ++files_in_flight ));
                if (( $files_in_flight == $THREADS )); then
                    echo "Waiting for jobs to finish"
                    wait $(jobs -p)
                    files_in_flight=0
                fi
            done

            wait $(jobs -p)
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -l ${LIBS} build/objects/*.o ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o -o build/${EXE_NAME}
        elif [ "$MODE" = "release" ]; then
            files_in_flight=0
            for file in ${CXX_FILES[*]}; do
                echo $file
                clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -I ${INCLUDE} ${file} -c -o build/objects/$(basename ${file}).o &
                (( ++files_in_flight ));
                if (( $files_in_flight == $THREADS )); then
                    echo "Waiting for jobs to finish"
                    wait $(jobs -p)
                    files_in_flight=0
                fi
            done

            wait $(jobs -p)
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -l ${LIBS} build/objects/*.o -o build/${EXE_NAME}
        fi
        type ./thirdparty/tools/ctime.exe && ./thirdparty/tools/ctime.exe -end ./build/timings.ctm
    else
        if [ "$MODE" = "debug" ]; then
            files_in_flight=0
            for file in ${CXX_FILES[*]}; do
                echo $file
                clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -I ${INCLUDE} ${file} -c -o build/objects/$(basename ${file}).o &
                (( ++files_in_flight ));
                if (( $files_in_flight == $THREADS )); then
                    echo "Waiting for jobs to finish"
                    wait $(jobs -p)
                    files_in_flight=0
                fi
            done

                    wait $(jobs -p)
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_DEBUG} -l ${LIBS} build/objects/*.o ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o -o build/${EXE_NAME}
        elif [ "$MODE" = "release" ]; then
            files_in_flight=0
            for file in ${CXX_FILES[*]}; do
                echo $file
                clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -I ${INCLUDE} ${file} -c -o build/objects/$(basename ${file}).o &
                (( ++files_in_flight ));
                if (( $files_in_flight == $THREADS )); then
                    echo "Waiting for jobs to finish"
                    wait $(jobs -p)
                    files_in_flight=0
                fi
            done

            wait $(jobs -p)
            clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME_RELEASE} -l ${LIBS} build/objects/*.o -o build/${EXE_NAME}
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
    rm -f ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o
    if [ "$OS" = "linux" ]; then
        for file in ${IMGUI_LINUX_CXX_FILES[*]}; do
            echo $file
            clang++ ${file} ${CXX_FLAGS} ${CXX_FLAGS_IMGUI} -I ${INCLUDE} -c -o ${IMGUI_OBJECT_FILES_DIRECTORY}/$(basename ${file}).o &
        done;

        wait $(jobs -p)
        exit 0
    elif [ "$OS" = "win32" ]; then
        for file in ${IMGUI_CXX_FILES[*]}; do
            echo $file
            clang++ ${file} ${CXX_FLAGS} ${CXX_FLAGS_IMGUI} -I ${INCLUDE} -c -o ${IMGUI_OBJECT_FILES_DIRECTORY}/$(basename ${file}).o &
        done;

        wait $(jobs -p)
        exit 0
    fi
fi

echo Nothing to do
