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
- Separate game code and engine code
- Change world scale- maybe 32x18 tiles in a screen?
- Change player size
- - This will include updating the collision code to work for non-square sizes
- Follow camera
- Tilemap editor
- - It will allow you to "create a new tile" where you assign a texture that you load into the editor
- - The tilemap editor will export (compressed?) tilemap files
- - It will also export a hpp file that you can include in the game code
- - - The hpp file will have the embed directives for embedding the tilemap files, a struct containing all of the names of the textures you imported (all are fields of type Ichigo::TextureID), and a function to load all the textures.

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