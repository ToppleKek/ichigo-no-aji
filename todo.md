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
- ~~Improve ImGui debug window~~
- - ~~Make two panels on each side of screen?~~
- - ~~Press a key to toggle the visibility of the debug windows?~~
- - ~~Entity list viewer (shows active, inactive entities; allows you to focus/kill them)~~
- Change world scale- maybe 32x18 tiles in a screen?
- - Make world scale configurable by the game (right now it is hardcoded into the engine)
- ~~Change player size~~
- - ~~This will include updating the collision code to work for non-square sizes~~
- ~~Entity collisions~~
- - ~~Check for collisions between every active entity (maybe only ones within a certain distance from the player)~~
- - ~~Resolve collisions by calling a collision handler on each entity~~
- ~~Audio setup (DirectSound? XAudio?)~~
- ~~Audio mixer~~
- ~~Ichigo::push_render_command for allowing the game to draw the UI~~
- ~~Controller input (DirectInput? XInput?)~~
-~~ Tilemap editor~~
- - ?? (Optional) It will allow you to "create a new tile" where you assign a texture that you load into the editor
- - The tilemap editor will export (compressed?) tilemap files
- - ?? (Optional) It will also export a hpp file that you can include in the game code
- - - The hpp file will have the embed directives for embedding the tilemap files, a struct containing all of the names of the textures you imported (all are fields of type Ichigo::TextureID), and a function to load all the textures.
- ~~Font rendering with stb_truetype~~
- Localization support
- Basic pathfinding/AI
- - Uses the "Ichigo::EntityControllers" setup
- 2D Rigid body physics?
- ~~Black bar/windowing of aspect ratios out of range~~
- ~~Linux platform layer: fill out remaining key input~~
- ~~All platforms: support mouse input~~
- Render order?
- Animations?
- Better asset system? I.e. tie multiple sprites to one asset, entity has "texture asset" or something so you can have multiple sprites make up one entity
- Just like there is a tile texture map, there needs to be a way for the game to specify tile properties
- - Set certain tiles to be intangible, etc.

## The actual TODO for things that need to be done to ship
- ~~Text rendering (ASCII and hiragana/katakana, maybe some kanji?)~~
- ~~SIMDify mixer~~
- ~~Looping audio in mixer~~
- Animation "system"
- - ~~Spritesheets~~
- - ~~Animations~~
- - Frame skip
- - ~~Horizontal flipping~~
- - ~~Entry/exit animations~~
- - - ~~Add `cell_of_loop_start`~~
- - - ~~Add `cell_of_loop_end`~~
- Tiles use spritesheet
- Particle system

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

## Third-party Dependencies
- Dear ImGui (Development only)
- SDL2 (Linux only, development only as it will be replaced if I decide to actually ship on Linux)
- stb_image (Public domain)
- stb_truetype (Public domain)
- dr_mp3 (Public domain)

## Software used
- git (source control)
- clang (compiler)
- RemedyBG (debugger)
- Focus (editor)
- RenderDoc (Graphics debugger)
- NVIDIA Nsight graphics (Graphics debugger)
- Ctime (Compile time analysis tool)

## Project Objective
### The problem
Current game engines are, in all honesty, not that appealing. For a single programmer wanting to get started making a game, none of the game engines currently available offer enough benefits to outweigh the negatives of using them. Writing everything from scratch is more appealing. There are certain features that game engines offer that would be nice, but often it is just not enough to sway my opinion on them (Insert Unity rant here).
### Why not...
Unreal? Powerful engine, but I am a single developer and I do not want to use such a huge clunky thing for a simple indie game.

Unity? I would love to program my game in a real programming language.

Godot? Same problem as Unity.
### What?
A simple, minimal dependency game engine for 2D side scrolling games. Built for indie programmers who don't want to deal with garbage.
### Why?
"If no one does it civilization will collapse."
- Interesting
- Has much less complexity over existing engines.
- - No scripting languages, no BS. Just write the game code.
- Written from scratch; no licensing.
- Ultimate control over everything in the project

## Interesting things to put in presentation
### Original audio mixer
"It's great to have all of these ideas on how to structure your program, but if you don't have a program yet then you don't have a structural problem to solve yet."

The software design methodology: "rapid understanding"
- Start with a real problem statement: I want to be able to play sounds and music in my game.
- What do you need to make this a reality? Break it down: I need an audio mixer.
- Keep breaking it down: I need a way to play audio samples.
- Notice that this is a platform issue. Okay, now: I need a way to communicate the sounds I want to play to the platform layer
Finally we have two problems to solve:
- I need a way to play samples (in the platform layer)
- I need a way to send audio samples to the platform layer

The first audio mixer:
```c++
void Ichigo::Internal::fill_sample_buffer(u8 *buffer, usize buffer_size, usize write_cursor_position_delta) {
    static u8 *DEBUG_ptr = music_buffer;
    ICHIGO_INFO("write_cursor_position_delta=%llu", write_cursor_position_delta);
    DEBUG_ptr += write_cursor_position_delta;
    std::memcpy(buffer, DEBUG_ptr, buffer_size);
}
```
## Timetable
### Week 1
- ~~Proof of concept~~
### Week 2
- ~~Framerate limiter~~
- ~~Splitting of engine/game code~~
### Week 3
- ~~More splitting of engine/game code~~
- ~~General code refactoring, planning of game to engine interface~~
### Week 4
- ~~Camera system and a follow camera that keeps any specified entity on screen~~
- ~~Better in-engine debug information (game data and engine controls in panel on left of screen- this panel can be toggled)~~
- ~~Player movement/map collision detection rework~~
- - ~~Small error correction/ignoring of float imprecision~~
- - ~~Make player speed, gravity, friction, etc. configurable~~
- - ~~Improve standing on ground detection algorithm~~
- - ~~Generalize player movement code and map collision detection so that other entities can use the same code~~
### Week 5
- ~~Make size of visible entity sprite not tied to the size of its collider~~
- ~~Basic movement of other entities (akin to how eg. Goombas move back and forth in Mario)~~
- ~~Entity collision detection and resolution~~
- ~~Audio setup (win32: DirectSound, or XAudio2? linux: ALSA)~~
- - Port this to linux? Not sure if the ALSA interface works well with the method we are using on win32
- ~~Basic audio mixer~~
- - ~~Plan Audio mixer interface~~
### Week 6
- ~~Finish introduction in report~~
- ~~Implement planned push_render_command() function~~
- - Allows the game to draw primitives/text (text rendering planned for a later week) for a UI
- Improve audio mixer
- - ~~Platform pause playback~~
- - ~~Allow canceling specific sounds~~
- - ~~Volume control~~
- - ~~Pan control~~
- - Pitch control
- - Fading effect
- - SIMD
- - Streaming from disk (?)
- Catchup on anything not completed
- Design a unit test system for the game
- - Eg. ensure spawning entities always works, requesting a entity that was killed correctly rejects the request, etc.
- ~~Support controller input~~
- ~~Draw backgrounds~~
### Week 7
- ~~Literature review~~
- - https://www.dr-lex.be/info-stuff/volumecontrols.html#about
- - https://dl.acm.org/doi/pdf/10.1145/971564.971590
- Text rendering with stb_truetype
- Begin work on a tilemap editor
- - It will allow you to "create new tile types" where you assign a texture that you load into the editor
- - The tilemap editor will export (compressed?) tilemap files
- - It will also export a hpp file that you can include in the game code
- - The hpp file will have the embed directives for embedding the tilemap files (The direct embedding approach, thinking about this still), a struct containing all of the names of the textures you imported (all are fields of type Ichigo::TextureID), and a function to load all the textures.
### Week 8
- More tilemap editor work
- ~~Main goal and rationale~~
- Quite busy this week, might not get much done
### Week 9
- More tilemap editor work
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
