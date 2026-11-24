# Hunter's Assassin

Milestone 1 is a 2D top-down movement prototype with a handcrafted 30 by 20 tile maze. It opens a 1280 by 720 window and targets 60 FPS.

## Build on Windows

Install Visual Studio or Build Tools with the **Desktop development with C++** workload, CMake 3.20 or newer, and Git. Run these commands from the repository root in a Developer PowerShell for Visual Studio:

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\Debug\HunterAssassin.exe
```

The commands use CMake's default Visual Studio generator. Visual Studio can also open the repository via **File > Open > Folder**. Select `HunterAssassin.exe` as the startup target.

In VS Code, install the **C/C++** and **CMake Tools** extensions. Run **CMake: Select a Kit**, choose your MSVC kit, and run **CMake: Configure**. Select `HunterAssassin` as the launch target, then press F5.

The first configure downloads raylib 5.5. For offline builds with an existing raylib 5.5 source checkout, add `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=C:/path/to/raylib` to the configure command. CMake copies the map beside the executable in `maps/`, so the game can launch from the IDE, the build directory, or another working directory.

## Controls

- **WASD** moves the player. The dark mark inside the circle indicates facing.
- **F3** toggles the world grid, FPS, world position, and zero-based tile coordinates (column, row).
- **Esc** closes the game.

The HUD displays three health points and a placeholder escape objective.

## Project layout

- `src/main.cpp` starts the game and reports startup errors.
- `src/Game.*` owns the game loop, camera, HUD, and debug overlay.
- `src/Player.*` handles normalized input, movement, facing, and wall collision.
- `src/TileMap.*` loads, validates, and draws the maze. The player checks nearby wall tiles using the closest point on each tile rectangle and resolves the two movement axes separately.
- `maps/test_map.txt` defines walls (`#`), floors (`.`), and the single player spawn (`S`). Each tile is 48 world units.
