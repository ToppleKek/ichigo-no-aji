# Ichigo! + MUSEKININ Society

*Ichigo!* is a simple, from scratch, minimal dependency game engine for 2D side scrolling games.

*MUSEKININ Society* is an action platformer demo game.

## Building

On Windows you must have MSYS2 or git bash installed.

Use: `build.sh build <PLATFORM> <TYPE> <P>` where `<PLATFORM>` is either `win32` or `linux`, `<TYPE>` is either `debug` or `release`, and `<P>` is the number of build processes to spawn.

## Controls
### Game
- ENTER - Menu accept
- Z - (midair) Dive
- X - Cast spell
- TAB - Switch spells
- SPACE - Jump
- Arrow keys: Move
- SHIFT - Hold to run

### Debug mode cheats
- CTRL+N - Toggle noclip
- CTRL+M - Give money
- CTRL+1 - Load level 1
- CTRL+2 - Load level 2
- CTRL+3 - Load level 3
- CTRL+UP - Increase timescale
- CTRL+DOWN - Decrease timescale
- CTRL+LEFT/RIGHT - Reset timescale to 1.0
- CTRL+R - Hard reset level
- F1 - Toggle level editor
- F3 - Toggle debug menu

### Editor
- F - Fill selection with brush tile
- E - Fill selection with air tile (erase)
- CTRL+Z - Undo
- CTRL+SHIFT+Z - Redo
- CTRL+S - Save as
- CTRL+N - New tilemap
- W/A/S/D - Move camera
- SPACE - Hold to increase keyboard camera speed
- RIGHT MOUSE - Pick tile (eyedropper tool)
- LEFT MOUSE - Click or drag to select a region
- MIDDLE MOUSE - Click and hold to pan
- SCROLL - Zoom camera
- M - Toggle entity move mode when an entity descriptor is selected
- CTRL+D - Duplicate the selected entity descriptor