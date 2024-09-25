# Unnamed Mystery Dungeon Type Game OR "Ichigo no Aji!" OR kekslop magical girl game OR simple single shade (puzzle?) game

## Programming (vulkan)
- ~~Draw triangle~~
- ~~Fix vkfence validation error~~
- ~~Frames in flight~~
- ~~Rebuild the swapchain on window resize~~
- ~~Draw rectangles~~
- **REVIEW ALL VULKAN SETUP**
- Staging buffer (?)
- Uniform buffers
- Draw textured rectangles
- Get user input

## Game
- ~~Textured rectangles~~
- ~~World tile format, draw world~~
- ~~Collision detection~~
- ~~Wall gliding~~
- ~~jumping~~
- ~~Redo movement (!!) (Some things are not framerate independent, forgot a dt somewhere?)~~
- ~~Review collision, you can get stuck~~
- ~~Separate game code and engine code~~
- ~~Follow camera~~
- Improve ImGui debug window
- - Make two panels on each side of screen?
- - ~~Press a key to toggle the visibility of the debug windows?~~
- - Entity list viewer (shows active, inactive entities; allows you to focus/kill them)
- Change world scale- maybe 32x18 tiles in a screen?
- - Make world scale configurable by the game (right now it is hardcoded into the engine)
- Change player size
- - This will include updating the collision code to work for non-square sizes
- Entity collisions
- - Check for collisions between every active entity (maybe only ones within a certain distance from the player)
- - Resolve collisions by calling a collision handler on each entity
- Audio setup (DirectSound? XAudio?)
- Audio mixer
- Ichigo::push_render_command for allowing the game to draw the UI
- Controller input (DirectInput? XInput?)
- Tilemap editor
- - It will allow you to "create a new tile" where you assign a texture that you load into the editor
- - The tilemap editor will export (compressed?) tilemap files
- - It will also export a hpp file that you can include in the game code
- - - The hpp file will have the embed directives for embedding the tilemap files, a struct containing all of the names of the textures you imported (all are fields of type Ichigo::TextureID), and a function to load all the textures.
- Font rendering with stb_truetype
- Localization support
- Basic pathfinding/AI
- - Uses the "Ichigo::EntityControllers" setup
- 2D Rigid body physics?

## Wishlist
- Fixed number of timesteps? (fixed ~200hz physics?)

## Art
- I like this games pixel art: https://store.steampowered.com/app/2220360/Paper_Lily__Chapter_1/

## Report
- Report about a 2D sidescroller engine providing:
- - P.I. (Platform independent) Rendering
- - P.I. Audio
- - P.I. Input
- - Player controller and collisions
- - 2D Rigid body physics?
- - Text & translation?
- evolutionary prototyping?
- Diagrams of the game loop, or whatever

## External Dependencies
- Dear ImGui (Development only)
- SDL2 (Linux only, development only as it will be replaced if I decide to actually ship on Linux)
- stb_image
- stb_truetype

## Software used
- clang (compiler)
- RemedyBG (debugger)
- VSCode (editor)
- RenderDoc (Graphics debugger)
- NVIDIA Nsight graphics (Graphics debugger)

## Timetable
### Week 1
- Proof of concept
### Week 2
- Framerate limiter
- Splitting of engine/game code
### Week 3
- More splitting of engine/game code
- General code refactoring, planning of game to engine interface
### Week 4
- Camera system and a follow camera that keeps any specified entity on screen
- Better in-engine debug information (game data and engine controls in panel on left of screen- this panel can be toggled)
- Player movement/map collision detection rework
- - Small error correction/ignoring of float imprecision
- - Make player speed, gravity, friction, etc. configurable
- - Generalize player movement code and map collision detection so that other entities can use the same code
- Basic movement of other entities (akin to how eg. Goombas move back and forth in Mario)
### Week 5
- Entity collision detection and resolution
- Audio setup (win32: DirectSound, or XAudio2? linux: ALSA)
- Basic audio mixer
- - Plan Audio mixer interface
- Implement planned push_render_command() function
- - Allows the game to draw primitives/text (text rendering planned for a later week) for a UI
### Week 6
- Catchup on anything not completed
- Text rendering with stb_truetype
- Design a unit test system for the game
- - Eg. ensure spawning entities always works, requesting a entity that was killed correctly rejects the request, etc.
- Support controller input
- Draw backgrounds
### Week 7
- Begin work on a tilemap editor
- - It will allow you to "create new tile types" where you assign a texture that you load into the editor
- - The tilemap editor will export (compressed?) tilemap files
- - It will also export a hpp file that you can include in the game code
- - The hpp file will have the embed directives for embedding the tilemap files (The direct embedding approach, thinking about this still), a struct containing all of the names of the textures you imported (all are fields of type Ichigo::TextureID), and a function to load all the textures.
### Week 8
- More tilemap editor work
- Catchup, Start seriously putting together a report (I just have a bunch of notes at this point probably)
- Quite busy this week, might not get much done
### Week 9
- Start thinking about localization support
- - String tables
- Try to support ruby characters? (furigana)
- - (Small characters that appear above text as a reading aid for certain languages, eg. Japanese)
- Presentation/report
### Week 10
- Basic pathfinding/AI
- - Uses the "Ichigo::EntityControllers" setup
- 2D Rigid body physics? Not sure if we want to include this or not yet
- Presentation/report
### Week 11
- Presentation/report cleanup
### Week 12
- Presentation/report cleanup
### Week 13