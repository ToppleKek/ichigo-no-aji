set -e

#-Wall -Wextra -Wpedantic -Wconversion
CXX_FLAGS="-std=c++20 -Wall -Wextra -fno-exceptions -Wno-deprecated-declarations -Wno-missing-braces"
CXX_FLAGS_GAME="-g"
CXX_FLAGS_IMGUI="-O3"
CXX_FILES="main.cpp win32_ichigo.cpp util.cpp"
CXX_FILES_LINUX="main.cpp linux_ichigo.cpp util.cpp"
IMGUI_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_win32.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
IMGUI_LINUX_CXX_FILES=(./thirdparty/imgui/imgui.cpp ./thirdparty/imgui/imgui_draw.cpp ./thirdparty/imgui/imgui_tables.cpp ./thirdparty/imgui/imgui_widgets.cpp ./thirdparty/imgui/imgui_impl_sdl2.cpp ./thirdparty/imgui/imgui_impl_opengl3.cpp)
LIBS="user32 -lwinmm -lopengl32"
LIBS_LINUX="GL -lSDL2"
EXE_NAME="game.exe"
IMGUI_OBJECT_FILES_DIRECTORY="build/imgui"
INCLUDE="thirdparty/include"

mkdir -p build

if [ "${1}" = "run" ]; then
    cd build
    ./$EXE_NAME
    exit 0
fi

if [ "${1}" = "br" ]; then
    clang++ ${CXX_FLAGS} ${CXX_FLAGS_GAME} -l ${LIBS} -I ${INCLUDE} ${CXX_FILES} ${IMGUI_OBJECT_FILES_DIRECTORY}/*.o -o build/${EXE_NAME}
    cd build
    ./$EXE_NAME
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
