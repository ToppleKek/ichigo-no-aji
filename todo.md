# Ichigo! and Ichigo no Aji!

# MUSEKININ
- Moving to a new school
- Start out in the dorm
- Objective 1 go to your room
- Next day, go to school. See club day. Find decrepit stall: MUSEKININ society
- Talk to them, they tell you to meet them back here tonight, disappear
- Go back at night
- You are now part of their club. You get a wand. They have no idea what to do with it.
- You are told to explore a hidden ruins under the school
- When you go under the school, there is almost like a little hub area that has entrances to different parts of the ruins
- The only one that is accessable is one that has no obstacles, the others are locked behind gimmicks you can't solve yet.
- You go in the first one, you find that using the wand you can activate special switches
- You can also use a range shot. Charging is optional, but does extra damage.
- Switches can do anything from moving blocks to form a path, or activating traps to kill enemies
- You make your way through the stage, and at the end there is a small boss fight
- Upon beating the boss, you obtain information on how to use the wand. You get a new spell.
- The first spell you learn is a fire spell. This allows you to melt blocks of ice, and is more effective at killing certian kinds of enemies

## TODO (musekinin)
### GRAPHICS
- ~~Moving platform needs graphics + custom render proc to render different sized platforms~~
- - ~~world_render_vertex_list()~~
- - The platform will render two end tiles (or a 1x1 tile version) and a repeating middle tile
- ~~Render order~~
- Animate new player character
- Animate new enemy
- background graphics for dorm, store, etc
- Better magic graphics

### OTHER ASSETS
- Select music, sound effects
- Design levels

### PROGRAMMING
- Allow casting magic in more states (falling, jumping)
- Particle system
- ~~Save format~~
- - Currency? For the shop and stuff
- Cutscenes
- - Cutscene trigger entity
- - On collide, player checks level flags in save file to see if the cutscene has already played. If not, disable input and view the cutscene.
- ~~Bezier curves for camera smoothing~~
- Shop
- ~~Implement health~~
- Moving platform has weird physics
- Camera guide entities (blockers)


### SAVE FORMAT
NOTE: *Array<T>* is usize size, T items
u16 VERSION NUMBER
<PLAYER INFO>
u16 health
i64 level
Vec2<f32> position
u64 inventory_flags
u64 story_flags
Array<LevelInfo>
<LEVEL INFO> structs as follows
u64 progress_flags
u64 item_flags


## Scope of the school part of the project
- Only a demo. First 2 levels at least + hub world.

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

## The actual TODO for things that need to be done to ship
- ~~Text rendering (ASCII and hiragana/katakana, maybe some kanji?)~~
- ~~SIMDify mixer~~
- ~~Looping audio in mixer~~
- ~~Animation "system"~~
- - ~~Spritesheets~~
- - ~~Animations~~
- - ~~Frame skip~~
- - ~~Horizontal flipping~~
- - ~~Entry/exit animations~~
- - - ~~Add `cell_of_loop_start`~~
- - - ~~Add `cell_of_loop_end`~~
- ~~Tilemap editor saving to a file~~
- ~~Tiles use spritesheet~~
- ~~Adding new tiles to tileset editor~~
- ~~Render all tiles in one draw call~~
- Perfect pixel graphics (Casey lecture)
- Look into weird texture shifting on player sprite in specific x positions (float error?)
- Increase tile name length
- Tileset editor UI revamp
- Particle system
- Level format (include entities?)
- More level editor features
- - Pick tile
- - Move selection
- - Duplicate selection

## Wishlist
- Fixed number of timesteps? (fixed ~200hz physics?)

> Just like there is a tile texture map, there needs to be a way for the game to specify tile properties
> Eg. set certain tiles to be intangible, etc.

This is done! But, is it kind of stupid to make a "new tile" (new tile ID) for every tile that has the same properties, just a different texture?
Something to think about. Maybe there is a "tileset" that can get swapped out depending on the level.

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
