# Hunter's Assassin code walkthrough

This walkthrough covers the source at the end of Milestone 5B.2. Update it with each milestone. Links to source files are relative to this document.

## 1. Project overview and architecture

Hunter's Assassin is a C++17 top-down stealth prototype. raylib supplies the window, input, frame timing, camera, and drawing. Milestone 1 introduced the maze and player loop. Milestone 2 added two independent guards, vision, and shortest-path navigation. Milestone 3 added combat and exposure. Milestone 4A added the facility and seven patrols. Milestone 4B.1 added cards, doors, and a chained hash index. Milestone 4B.2 added terminals and the Protocol Zero override. Milestone 4B.3 added the countdown and mission results. Milestone 4C added the minimap. Milestone 5A added alarms and a heap-based reinforcement scheduler. Milestone 5B.1 added laser combat. Milestone 5B.2 adds character animation and sprite effects.

```text
main
  |
 Game ----> Player ---------> TileMap
  |           (input)           ^
  +-------> Guard --------------+  collision and vision
  |           |
  +-------> BfsPathfinder ------+  walkability queries
              ^
              +-- Guard requests routes using shared scratch storage
```

The map is the common source of truth for walls, including melee sightlines. Actors move continuously in world coordinates, and guards choose routes on the discrete tile grid. BFS (breadth-first search) finds a route to the tile selected by the guard's AI. `Game` sequences attacks and aggregates detection, `Guard` owns retaliation and hit direction, and `Player` owns health, immunity, and the exposure timer. The HUD shows current health and a placeholder escape objective.

## 2. Execution flow

1. **Entry.** `main()` constructs a local `Game` and calls `Run()`. It reports a caught `std::exception` on standard error and returns 1. Normal completion returns 0.
2. **Initialization.** `Game::Initialize()` loads the facility grid and object records from `assets/` beside the executable. It configures the camera and calls `ResetRound()` to spawn the player and validate seven guard routes. After opening the 1280 by 720 window it loads the level, combat, and shared animation textures once and targets 60 FPS.
3. **Update.** `Game::Run()` passes `GetFrameTime()` to `Game::Update()` while the window is open. This duration in seconds is called **delta time**. The update handles Escape and restart, ticks the countdown during an active mission, moves the player, updates pickups and nearby interactions, or handles terminal input while the player is stationary. It resolves the knife outside terminals, then advances in-flight projectiles, updates each guard, and checks for retaliation or a newly fired shot. It aggregates detection for the player's exposure timer and adjusts the camera. Timeout and fatal guard hits end the update before a queued exit can become victory. If the mission continues, it drains due events and checks for a newly detected player.
4. **Render.** `Game::Render()` draws the map, cones, optional debug paths, actors, and combat effects in camera mode. It exits camera mode before drawing the health, exposure, and access HUD, the screen-space minimap, debug text, countdown, and mission results. The HUD and defeat prompt use window-relative coordinates.
5. **Shutdown.** A close request, including Escape when no terminal is open, ends the loop. `Game::Shutdown()` unloads the textures before calling `CloseWindow()`. The local `Game` and its C++ members are destroyed when `main()` leaves the block.

## 3. File-by-file overview

| File | Responsibility and direct dependencies |
| --- | --- |
| [main.cpp](../src/main.cpp) | Program entry and exception reporting. Includes `Game.h` and the standard exception and stream headers. |
| [Game.h](../src/Game.h) | Declares the owning `Game` object, with `TileMap`, `Player`, `BfsPathfinder`, `Guard`, and raylib `Camera2D` members. Includes the four game component headers. |
| [Game.cpp](../src/Game.cpp) | Coordinates lifecycle, round reset, knife targeting, camera motion, and rendering. Includes `Game.h` and standard headers for clamping, math, paths, and exceptions. |
| [Player.h](../src/Player.h) | Declares `Player`, its movement data, health, and timers. Includes raylib for `Vector2` and forward-declares `TileMap`. |
| [Player.cpp](../src/Player.cpp) | Resolves WASD movement, manages damage and exposure, and draws the player. Includes `Player.h`, `TileMap.h`, and `<cmath>`. |
| [TileMap.h](../src/TileMap.h) | Declares `TilePosition` and its equality operators, and defines the `TileMap` interface. Includes raylib and standard filesystem, string, and vector headers. |
| [TileMap.cpp](../src/TileMap.cpp) | Parses the map, draws it, converts positions, and checks collision and sightlines. Includes `TileMap.h` and standard utilities for file input, math, limits, and exceptions. |
| [Guard.h](../src/Guard.h) | Declares `Guard`, state and navigation enums, route data, and tuning constants. Includes `TileMap.h` and standard container and size headers. Forward-declares `BfsPathfinder`. |
| [Guard.cpp](../src/Guard.cpp) | Implements patrol validation, state transitions, routing, vision, combat, and guard rendering. Includes `Guard.h`, `BfsPathfinder.h`, and standard math, algorithm, and exception headers. |
| [BfsPathfinder.h](../src/BfsPathfinder.h) | Declares the pathfinding interface, private `ArrayQueue`, and reusable buffers. Includes `TileMap.h`, `<cstddef>`, and `<vector>`. |
| [BfsPathfinder.cpp](../src/BfsPathfinder.cpp) | Implements the queue, graph traversal, and route reconstruction. Includes `BfsPathfinder.h`, `<algorithm>`, and `<cassert>`. |
| [EventHeap.h](../src/EventHeap.h) | Fixed-array priority queue for reinforcement spawns and alarm expiry. |
| [DoorHash.h](../src/DoorHash.h) | Owns the separate-chaining index from tile keys to logical door indices. |
| [CharacterAnimation.h](../src/CharacterAnimation.h) | Shared frame-array layout and per-character animation playback. |
| [Artwork.h](../src/Artwork.h) | Uses raylib to draw sprites at native size around their world-space centers. |
| [Facility assets](MAP_DESIGN.md) | The active 50 by 35 map, object records, PNGs, and the seven patrol routes. |
| [.vscode/tasks.json](../.vscode/tasks.json) and [.vscode/launch.json](../.vscode/launch.json) | The current Windows workflow. F5 runs the `Build Raylib` task, which compiles all six source files with `g++ -std=c++17`. The launch configuration starts the resulting workspace-root `game.exe` with GDB. |
| [CMakeLists.txt](../CMakeLists.txt) | The older Milestone 1 target and map-copy rule. Its source list lacks `Guard.cpp` and `BfsPathfinder.cpp`, so use the VS Code task for the current game. |

## 4. Class relationships and important data

**Ownership** means an object's lifetime is tied to its containing object. `Game` owns these components as members. The player and guards receive references to the map for individual calls. Initial and reinforcement guards use the same pathfinder sequentially and retain separate routes and state.

| Type | Important data |
| --- | --- |
| `Game` | `map_`, `player_`, `pathfinder_`, `guards_`, `camera_`, and `debugVisible_`. `objects_` holds placements, while `facilityTexture_` and `sprites_` hold GPU texture handles. `knifeCooldown_` gates swings. `knifeEffect_`, `knifeOrigin_`, and `knifeTarget_` store the visual effect. |
| `TilePosition` | Integer `column` and `row`. The two equality operators support route and target comparisons. |
| `TileMap` | `tiles_` is a vector of row strings. `crates_` holds a flat occupancy flag per cell. `spawn_` is used by the legacy prototype loader. The facility spawn comes from `objects_`. `TileSize` is 48 world units. |
| `Player` | `position_` and unit direction `facing_`, initially down. `Radius` is 15 and base `Speed` is 220 units per second. `health_` starts at `MaxHealth` (3). `immunity_` and `exposure_` count down in seconds. |
| `Guard` | `patrol_` and `waypoint_` store the patrol and goal index. `path_`, `pathIndex_`, `tile_`, `stepping_`, and `plannedTarget_` track the cached route and current segment. `position_` and `facingAngle_` drive the body and cone. `state_`, `suspicion_`, `searchTime_`, and `lastKnown_` track detection. `health_` starts at 2. `attackCooldown_` and `attackEffect_` track retaliation timing. |
| `BfsPathfinder` | `frontier_` is the custom FIFO queue. `parents_` combines visit markers and predecessors. |
| `BfsPathfinder::ArrayQueue` | `cells_` stores integer tile IDs in contiguous slots. `head_` is the next read index and `tail_` the next write index. |

## 5. Function and method reference

The signatures below include parameter types, return types, and `const` qualifiers. A row covers what the function does, why it exists, and its direct callers. Algorithm details follow in the later sections.

### Entry and game

| Signature | Purpose and callers | Concept |
| --- | --- | --- |
| `int main()` | C++ entry point. Runs `Game::Run()` and translates exceptions into a diagnostic and failure exit code. | Lifecycle and error handling |
| `void Game::Run()` | Called by `main()`. Initializes once, repeatedly updates and renders, then shuts down. | Game loop |
| `void Game::Initialize()` | Called by `Run()`. Loads map and entities before opening the window and configuring the camera. | Initialization and validation |
| `void Game::LoadObjects(const std::filesystem::path& path)` | Called at startup to parse placements, set crate occupancy, and validate reachability with BFS. | Stream parsing and graph reachability |
| `void Game::LoadArtwork(const std::filesystem::path& directory)` | Called after window creation. Loads one base image and one PNG per object type. | Texture lifecycle |
| `void Game::DrawFacility() const` | Called by `Render()`. Draws the base at world (0, 0) and draws static props from placement records. | Tile-to-world mapping |
| `void Game::ResetRound()` | Called by `Initialize()` and by `Update()` when R is pressed after victory or defeat. Resets actors, knife timers, and camera using the loaded map. | State reset |
| `void Game::Update(float deltaTime)` | Called by `Run()`. Handles input and defeat, sequences movement and combat, refreshes exposure, and follows the player. | Simulation order and interpolation |
| `void Game::AttackWithKnife()` | Called by `Update()` on an eligible Space press. Finds the nearest living, reachable guard and records the swing for rendering. | Linear minimum search |
| `Vector2 Game::ClampCameraTarget(Vector2 target) const` | Called by `ResetRound()` and `Update()`. Keeps the viewport within large map axes and centers smaller ones. | Range clamping |
| `void Game::Render() const` | Called by `Run()`. Produces world and HUD layers in drawing order. | Coordinate spaces |
| `void Game::Shutdown()` | Called by `Run()` after the loop. Closes the raylib window. | Cleanup |

### Player and map

| Signature | Purpose and callers | Concept |
| --- | --- | --- |
| `void Player::Spawn(Vector2 position, float rotation)` | Called by `Game::ResetRound()` to place the player, reset facing from the object rotation, restore health, and clear timers. | Initial state |
| `void Player::Update(float deltaTime, const TileMap& map, bool movementEnabled)` | Called by `Game::Update()`. Turns input into movement while retaining wall contact and slide behavior. | Vector normalization, substeps, axis separation |
| `void Player::Draw(Texture2D texture, const CharacterFrames* frames) const` | Called by `Game::Render()`. Draws a centered sprite rotated to face the current movement direction. A circle is the fallback when no texture is passed. | Vector drawing |
| `Vector2 Player::Position() const` | Read by `Game::ResetRound()`, `Update()`, `AttackWithKnife()`, and `Render()` for camera, combat, vision, or debug output. | Getter |
| `int Player::Health() const` | Used by `Game::Update()` and `Render()` to detect defeat and display health. | Getter |
| `bool Player::IsExposed() const` | Used by `Player::Update()`, `UpdateExposure()`, and `Game::Render()`. A positive timer means the slowdown and warning apply. | Timer predicate |
| `void Player::UpdateExposure(float deltaTime, bool detected)` | Called by `Game::Update()` after the guards act. Refreshes the timer during detection and counts it down during escape. | Shared condition and countdown |
| `void Player::TakeDamage(int damage)` | Called by `Game::Update()` after a guard swing. Removes one HP and starts immunity when the player can take damage. | State guard and countdown |
| `void TileMap::Load(const std::filesystem::path& path)` | Loads the legacy prototype maze and embedded spawn through `LoadGrid()`. | 2D grid parsing |
| `void TileMap::LoadFacility(const std::filesystem::path& path)` | Called by `Game::Initialize()`. Uses `LoadGrid()` to require a 50 by 35 facility grid. | 2D grid validation |
| `void TileMap::LoadGrid(const std::filesystem::path& path, bool facility)` | Shared parser behind both loaders. Validates the selected legend and initializes crate storage. | Row-major grid |
| `void TileMap::AddCrate(TilePosition tile)` | Called by `Game::LoadObjects()` to mark a walkable cell occupied. | Flat array indexing |
| `Vector2 TileMap::Spawn() const` | Returns the embedded spawn of the legacy prototype maze. | Getter |
| `int TileMap::Width() const` | Used by map wall queries and rendering, camera clamping, patrol buffer reservation, and BFS indexing. Returns the first row length or zero for an empty grid. | Grid dimension |
| `int TileMap::Height() const` | Used by the same systems as `Width()`. Returns the number of rows. | Grid dimension |
| `bool TileMap::IsWall(int column, int row) const` | Used by the overload, collision, ray traversal, and map drawing. Treats architectural walls, crate cells, and out-of-bounds cells as solid. | Bounds checking and constant-time lookup |
| `bool TileMap::IsWall(TilePosition tile) const` | Used by BFS, ray traversal, and melee sightline checks. Forwards a coordinate pair to the integer overload. | Convenience overload |
| `bool TileMap::CircleHitsWall(Vector2 center, float radius) const` | Called by `Player::Update()` and `Guard::Navigate()` to reject positions that overlap walls. | Bounding box and nearest point |
| `bool TileMap::HasClearLine(Vector2 from, Vector2 to) const` | Called by `Game::AttackWithKnife()` and `Guard::TryAttack()`. Validates both endpoints, then tests the segment so melee respects walls. | Vector normalization and ray traversal |
| `float TileMap::RayDistance(Vector2 origin, Vector2 direction, float limit, int ignoredDoor) const` | Called by `Guard::CanSee()`, `Guard::DrawVision()`, and `HasClearLine()`. Follows a unit direction to the first blocking boundary or returns the limit. | Grid ray traversal |
| `static TilePosition WorldTile(Vector2 world)` | `TileMap` conversion used by ray traversal, `Guard::Update()`, and game debug text. Identifies the cell containing a world point. | Floor division |
| `static Vector2 TileCenter(TilePosition tile)` | `TileMap` conversion used by `Load()` and guard construction, navigation, and debug drawing. Returns a waypoint's world center. | Grid-to-world mapping |
| `void TileMap::Draw() const` | Prototype rectangle renderer. The facility uses `Game::DrawFacility()`. | Row-major iteration |
| `void TileMap::DrawGrid() const` | Called by `Game::Render()` in debug mode to show tile boundaries and solid cells, with amber for crates. | Grid visualization |
| `inline bool operator==(TilePosition a, TilePosition b)` | Used by BFS and guard navigation to compare both integer coordinates. | Value equality |
| `inline bool operator!=(TilePosition a, TilePosition b)` | Used by guard navigation to detect a target change. Delegates to equality. | Value inequality |

### Guard

| Signature | Purpose and callers | Concept |
| --- | --- | --- |
| `Guard::Guard(const TileMap& map, BfsPathfinder& pathfinder, std::initializer_list<TilePosition> patrol, GuardType type)` | Called through `guards_.emplace_back(...)` in `Game::ResetRound()`. Copies and validates the patrol, places the guard, and chooses initial facing from the first path step when one exists. | Route connectivity and buffer reservation |
| `void Guard::Update(float deltaTime, Vector2 player, const TileMap& map, BfsPathfinder& pathfinder)` | Called once per guard by `Game::Update()`. Updates sight and state before performing the current state's actions. | Finite-state machine and timers |
| `bool Guard::CanSee(Vector2 player, const TileMap& map) const` | Called by `Guard::Update()`. Checks whether the player can contribute to suspicion or maintain detection. | Distance, dot product, and ray traversal |
| `Guard::Navigation Guard::Navigate(TilePosition target, float distance, const TileMap& map, BfsPathfinder& pathfinder)` | Called by `Guard::Update()` in moving states. Reuses or rebuilds a route, advances along the next segment, and reports moving, arrival, or failure. | Cached BFS, interpolation, collision |
| `Color Guard::AlertColor() const` | Used by `DrawVision()`, `Draw()`, and `DrawDebug()` to show alert status consistently. | Enum-to-color mapping |
| `const char* Guard::StateName() const` | Used by `DrawDebug()` for uppercase labels corresponding to the state enum. | Enum-to-label mapping |
| `void Guard::DrawVision(const TileMap& map) const` | Called by `Game::Render()` to show the wall-clipped vision cone as translucent triangles. | Sampled ray fan |
| `void Guard::Draw(Texture2D texture, const CharacterFrames* frames) const` | Called by `Game::Render()`. Draws the rotated guard sprite, alert symbols, and retaliation effect. Uses the old shape if no texture is passed. | State visualization |
| `void Guard::DrawDebug() const` | Called by `Game::Render()` in debug mode. Shows remaining path segments, waypoint rings, and the state label. | Route visualization |
| `void Guard::SetFacing(float degrees)` | Called by `Game::ResetRound()` to apply the initial object rotation. Later movement and AI update facing. | Degrees-to-radians conversion |
| `Vector2 Guard::Position() const` | Read by `Game::AttackWithKnife()` to measure range and save the slash endpoint. | Getter |
| `bool Guard::IsAlive() const` | Used by knife targeting, guard updates, drawing, hits, and `IsChasing()` to exclude guards at zero HP. | State predicate |
| `bool Guard::IsChasing() const` | Used by `Guard::TryAttack()` and `Game::Update()` to require a living guard in `State::Chase`. | State predicate |
| `Guard::AttackSide Guard::ClassifyAttack(Vector2 attacker) const` | Called by `TakeKnifeHit()`. Classifies the attack from the guard's facing and the direction to the attacker. | Dot product |
| `void Guard::TakeKnifeHit(Vector2 attacker)` | Called by `Game::AttackWithKnife()`. Applies direction-based damage and alerts a surviving guard. | Directional damage and state transition |
| `bool Guard::TryAttack(Vector2 player, const TileMap& map)` | Called by `Game::Update()` after guard AI updates. Checks chase, cooldown, range, and walls. Starts the swing effect and reports whether the player should attempt to take damage. | Predicates, ray traversal, and countdown |

### Sprite drawing

`inline void DrawCenteredSprite(Texture2D texture, Vector2 center, float degrees, Color tint)` draws using a half-width and half-height pivot. `Game::DrawFacility()`, `Player::Draw()`, and `Guard::Draw()` call it so rotation leaves the sprite centered on the same world point.

### BFS and the private queue

| Signature | Purpose and callers | Concept |
| --- | --- | --- |
| `bool BfsPathfinder::FindPath(const TileMap& map, TilePosition start, TilePosition destination, std::vector<TilePosition>& path)` | Called by the guard constructor and `Navigate()`. Clears the output and finds a shortest route. The route excludes the start and includes the destination when distinct. | BFS and predecessor reconstruction |
| `void BfsPathfinder::ArrayQueue::Reset(std::size_t capacity)` | Called by `FindPath()` to size or reuse storage and reset both indices. | Contiguous array storage |
| `void BfsPathfinder::ArrayQueue::Push(int cell)` | Called by `FindPath()` to enqueue a newly discovered tile. Asserts that a free slot exists. | FIFO enqueue |
| `int BfsPathfinder::ArrayQueue::Pop()` | Called by `FindPath()` to read the oldest pending tile. Asserts the queue is nonempty. | FIFO dequeue |
| `bool BfsPathfinder::ArrayQueue::Empty() const` | Used by `FindPath()` and the assertion in `Pop()`. Compares the read and write indices. | Empty-queue invariant |

## 6. Player movement and collision

`Player::Update()` subtracts opposite key states to get `(D - A, S - W)`. Positive `x` goes right and positive `y` goes down. Opposite keys cancel. When there is no input or time has not advanced, the player keeps its last facing and position.

A direction vector's length is computed with `std::hypot`. Dividing by this length produces a **unit vector**, which has length 1. W+D begins as `(1, -1)`, whose length is about 1.414. The normalized vector is approximately `(0.707, -0.707)`. Diagonal and straight movement therefore use the same base distance of `220 * deltaTime` before collision. While exposed, `ExposedSpeedFactor` in `Player.h` multiplies that distance by 0.7. Facing changes with input even when a wall blocks the requested movement.

The frame distance is split into steps of at most `Radius * 0.25`, or 3.75 units. This reduces **tunneling**, where a large movement step skips an obstacle. For each step, try the horizontal component, then try the vertical component from the resulting position. A failed horizontal test still permits vertical motion, which makes the player slide along the wall.

`TileMap::CircleHitsWall()` computes the tile bounds of the circle's enclosing square. For each wall in that small region it clamps the circle center's coordinates to the wall rectangle, obtaining its nearest point. If the squared distance to that point is less than the squared radius, the circle overlaps the wall. Squaring both sides avoids computing a square root. Exact tangency is allowed by the strict comparison.

## 7. Tile map, coordinates, and camera

The active grid uses `W` for walls, `.` for room floor, and `,` for corridor floor. `LoadFacility()` rejects any row length or row count other than 50 by 35. `LoadObjects()` reads the type, column, row, and rotation, rejects invalid or overlapping locations, and uses BFS to validate each spawn and visual prop from the assassin's tile after adding crates and indexing closed doors. Door art is validated against the logical door tiles. `IsWall()` combines the original grid, the separate crate flags, closed doors, and the boundary check. Existing circle collision, BFS, vision rays, and melee sightlines all call this same query.

```text
                  column / x increases ->
              +-------------+-------------+
row 0         | tile (0,0)  | tile (1,0)  |
              +-------------+-------------+
row 1         | tile (0,1)  | tile (1,1)  |
  |           +-------------+-------------+
  v  row / y increases
```

The storage order is `tiles_[row][column]`, while `TilePosition` holds `{column, row}`. The facility measures 2400 by 1680 world units. `WorldTile()` floors each world coordinate divided by 48, so `(-1, 0)` maps to an out-of-bounds column. `TileCenter()` multiplies each tile index plus 0.5 by 48. The assassin spawn at `(4, 4)` is world position `(216, 216)`.

`CameraZoom` in `Game.h` sets a 1.5× scale in the 1280×720 window. **World coordinates** describe gameplay positions and distances. **Screen coordinates** describe pixels in the window. Inside `BeginMode2D()`, raylib maps the camera target to the window center and scales world offsets by the zoom. A 48-unit tile therefore spans 72 screen pixels. Actors, vision cones, combat effects, and the in-world debug grid and paths scale together. HUD and debug readout text are drawn after `EndMode2D()` at fixed screen coordinates. The game currently has no mouse input.

`Game::Update()` moves its target a fraction `1 - exp(-8 * deltaTime)` toward the clamped player position. This **exponential interpolation** closes a consistent fraction of the remaining gap over a fixed elapsed time. `ClampCameraTarget()` divides the screen's half-width and half-height by zoom to get world-space edge margins. At 1.5× those margins are about 426.67 and 240 world units. On the facility the target ranges are approximately `x = 426.67..1973.33` and `y = 240..1440`. An axis shorter than the viewport is centered.

## 8. Guard patrol and state machine

A **finite-state machine** keeps one current state and uses events and timers to choose when to change it. A guard's constructor requires at least two waypoints and uses BFS to validate every leg, including the wraparound leg. Each guard starts at its first waypoint and initially targets its second. The routes are defined in `Game::ResetRound()`. A surviving guard can enter `Chase` directly from a knife hit. A guard at zero HP exits `Update()` before timers, vision, or AI run.

| Current state | Action and transitions |
| --- | --- |
| `State::Patrol` | Navigate to `patrol_[waypoint_]` at 95 units per second. Advance the index modulo the route size on arrival. Seeing the player starts `Suspicious` unless that frame already meets the detection threshold. |
| `State::Suspicious` | Pause navigation and keep facing. Accumulate visible time. At 0.65 seconds, enter `Chase`. Losing sight sooner clears suspicion and resumes `Patrol`. |
| `State::Chase` | Refresh `lastKnown_` every visible frame and navigate toward that tile at 165 units per second. Losing sight clears suspicion and the search timer, then enters `Search` immediately. |
| `State::Search` | Approach `lastKnown_` at 95 units per second. On arrival, or if navigation reports unreachable, rotate at `PI` radians per second and accumulate scan time. Enter `Return` after two seconds of scanning. Seeing the player at any time enters `Chase` immediately. |
| `State::Return` | Navigate to the active patrol waypoint at 95 units per second. On arrival, advance the route index and enter `Patrol`. Seeing the player enters `Chase` immediately. |

`Update()` checks visibility before the state action. On a frame where visibility changes the state, the new state's action can run immediately. The `Search` timeout sets `Return` at the end of the scan action, so movement toward patrol begins on a subsequent update unless the guard reacquires the player first.

`Navigate()` plans from `tile_`, the last center reached. If a goal changes while `stepping_` is true, the guard finishes the current segment before replanning. At a center it replans when the requested tile differs from `plannedTarget_` or the map's walkability revision changes. It moves a fraction of the distance to the next center, caps that fraction at 1, checks the proposed circle for overlap, and points along the step with `atan2`. Reaching a center updates `tile_` and advances `pathIndex_`. If the cached path is empty or exhausted, matching the target means arrival and otherwise means unreachable.

## 9. Guard vision and suspicion

1. **Range.** `CanSee()` computes the distance between actor centers. Beyond 210 units it returns false. Coincident positions return true.
2. **Angle.** Normalize the offset to the player. The guard's facing unit vector is `(cos(facingAngle_), sin(facingAngle_))`. The **dot product** multiplies matching components and adds the products. For unit vectors it equals the cosine of the angle between them. A dot product at least `cos(40 degrees)` places the target within the 80-degree cone.
3. **Line of sight.** `RayDistance()` computes where the ray next crosses a vertical or horizontal grid line. It advances through whichever boundary is closer. After crossing, it adds the constant distance to the next boundary on that axis. A zero direction component uses infinity so that axis is skipped. Ties within `0.0001` are corner crossings. The code checks both side cells before entering the diagonal cell, which blocks peeking through touching corners. Traversal stops at the first wall or the requested limit. `CanSee()` compares this result with the player distance.
4. **Suspicion.** Visible frames accumulate `deltaTime` in `suspicion_` while in patrol or suspicion. Losing sight resets the timer. Search and return reacquire immediately because this guard already detected the player during the current alert.
5. **Rendering.** `DrawVision()` samples 49 rays across 48 angular slices and joins neighboring endpoints into triangles. The sampled outline may approximate a wall edge between rays. The detection ray is traced directly to the player center. The guard remains red, with a yellow `?` during suspicion, a bright red `!` during chase, and an orange rim and cone during search.

## 10. Custom queue and BFS pathfinding

The maze is an **implicit graph**. A floor tile is a vertex, and a legal step to one of its four neighboring floor tiles is an edge. **Implicit** means the edges are calculated from the grid when needed. Every edge costs one step, so BFS is sufficient. BFS's first-in, first-out (FIFO) queue expands tiles in order of increasing distance in steps. The first discovery of a tile is through a shortest route.

### Queue and parent storage

`ArrayQueue` uses a `std::vector<int>` as its backing array and implements its own operations. `Reset(count)` resizes the array and sets `head_` and `tail_` to zero. `Push` writes at `tail_` and increments it. `Pop` reads at `head_` and increments it. `Empty` checks whether they are equal. There is no wraparound because each tile is enqueued at most once per search. Slots can be reused after reset, and queue operations cost constant time after reset.

`FindPath()` flattens a cell to `row * width + column`. It recovers the column with remainder and the row with integer division. `parents_[id] == -1` means the tile is undiscovered. The start is its own parent. For other cells, a valid parent both marks discovery and records the previous step. Marking a tile **at enqueue time** prevents two neighbors from adding it twice.

### Pseudocode

```text
FindPath(map, start, destination, path)
    clear path
    if start or destination is a wall or outside the map
        return false
    if start equals destination
        return true

    count = width * height
    first = start.row * width + start.column
    goal = destination.row * width + destination.column
    resize parents to count and fill with -1
    reset frontier to count slots
    parents[first] = first
    push first

    while frontier is not empty and parents[goal] equals -1
        current = pop frontier
        for each neighbor of current in order east, south, west, north
            skip walls and out-of-bounds tiles
            if parents[neighbor] equals -1
                parents[neighbor] = current
                push neighbor

    if parents[goal] equals -1
        return false
    for cell from goal back to first by following parents
        append cell's tile coordinates, stopping before first
    reverse path
    return true
```

The loop finishes the current neighbor pass when it discovers the goal. It does not have to dequeue the goal. If the destination is unreachable, the loop empties the queue and returns false with an empty path. A valid start equal to the destination also yields an empty path, but returns true. Callers use the boolean or their own arrival state to distinguish these cases.

For a general search, initialization touches all `W * H` grid slots. Traversal costs `O(V + E)` for the visited vertices and examined edges, and reconstruction costs linear time in the route length. Each vertex has at most four edges, so total worst-case time and auxiliary space are `O(W * H)`. The path output is also at most linear in the grid size. Guards reserve their path buffers up front, and repeated searches on the same map reuse the shared queue and parent storage.

### Dry run

Use this small teaching grid, where `A` and `B` mark floor cells in the example. A real map file uses its usual tiles and one `S`.

```text
     01234
  0  #####
  1  #A#B#
  2  #...#
  3  #####
```

The width is 5. A is `(1,1)`, ID 6. B is `(3,1)`, ID 8. The wall between them forces a detour.

| Action | Queue after action | Parent written |
| --- | --- | --- |
| Seed A | `[6]` | `parents_[6] = 6` |
| Pop 6, discover south | `[11]` | `parents_[11] = 6` |
| Pop 11, discover east | `[12]` | `parents_[12] = 11` |
| Pop 12, discover east | `[13]` | `parents_[13] = 12` |
| Pop 13, discover north | `[8]` | `parents_[8] = 13` |

Follow parents from B to get `8 -> 13 -> 12 -> 11 -> 6`. Drop 6 and reverse the remaining IDs. After decoding, `path` contains `(1,2), (2,2), (3,2), (3,1)`. That is a four-step route.

## 11. Example guard encounter

The first facility guard starts at `(24,10)` facing west. A player two cells west at `(22,10)` is in the guard's cone, 96 units away along the north corridor. If sight holds for 0.65 seconds, the guard pursues that tile. If the player breaks sight, the guard finishes navigating to the last seen tile, scans, then returns to its outstanding waypoint unless it detects the player again. See [the map design](MAP_DESIGN.md) for all seven routes.

## 12. Knife combat, health, and exposure

### Selecting a target

On a fresh Space press with `knifeCooldown_ <= 0`, `Game::AttackWithKnife()` starts a 0.45-second cooldown and a 0.16-second swing effect. It scans `guards_` once, ignoring eliminated guards and centers farther than 52 world units. It keeps the nearest candidate with a clear segment to the player. A blocked candidate leaves the best distance unchanged, so a farther clear guard can be selected. Equal distances keep the first eligible guard in vector order. This is a linear minimum search with constant extra storage, plus the cost of the wall rays it requests.

`TileMap::HasClearLine()` checks that both endpoints are floor, then sends a normalized ray through `RayDistance()`. Both melee attacks use this test, including the conservative corner rule. An idle swing still consumes cooldown and draws a ring. A hit also draws a line to the saved target position. The player can swing in any direction, since target selection depends on distance and visibility.

### Direction and retaliation

`Guard::ClassifyAttack()` takes the dot product of the guard's forward unit vector and the unit direction **from the guard to the player**. A value at most `-0.5` means rear, at least `0.5` means front, and values between those cutoffs mean side. Overlapping centers count as front. A rear hit sets health to zero. Front and side each remove one of the guard's two HP. A survivor faces the player, records their tile in `lastKnown_`, and enters `Chase`.

The knife resolves before guard updates. Eliminated guards stay in the vector and skip updates, attacks, and drawing. This keeps iteration stable. Actors have wall collision only, so an eliminated guard has no collision body to remove.

After a guard updates, `TryAttack()` requires a living guard in `Chase`, a ready cooldown, a clear line, and a player center within 44 units. A swing starts a one-second cooldown and shows an orange ring for 0.18 seconds. `Game::Update()` then calls `Player::TakeDamage()`. A swing consumes its cooldown even if the player's immunity absorbs it.

### Health, defeat, and exposed movement

`TakeDamage()` removes one HP when the player has health and their immunity has expired. It starts 0.8 seconds of immunity so two guards swinging on the same frame can remove only one HP. `Player::Update()` ticks the timer even when the player stands still, and `Draw()` tints the player while immune. The HUD draws the current health and three filled or empty pips.

On zero HP, the game freezes simulation and displays **MISSION FAILED** with the cause. Press **R** to restore all seven guards, the spawn, three player HP, and a fresh camera target and timers. Access cards return, all doors close, and interaction feedback clears. The window and loaded map stay available throughout the restart.

When any living guard ends its update in `Chase`, `Game::Update()` marks the frame detected. `Player::UpdateExposure()` refreshes the 1.25-second exposure timer while detected and counts it down otherwise. **EXPOSED** appears while the timer is positive. Movement uses 70 percent of base speed, or 154 world units per second, then returns to 220 when the timer expires. The player moves before guard updates, so changes to that timer affect movement on the following update.

## 13. DSA concepts in the current code

| Concept | Where to find it | Why it fits |
| --- | --- | --- |
| Two-dimensional grid | `TileMap::Load()`, `IsWall()` in `TileMap.cpp` | A row and column support direct tile access and predictable bounds checks. |
| Flat array indexing | `BfsPathfinder::FindPath()` | A single integer identifies a cell in both BFS buffers. |
| FIFO queue | `BfsPathfinder::ArrayQueue` | Older discoveries expand first, preserving BFS distance layers. |
| Implicit graph and BFS | `BfsPathfinder::FindPath()` | Four on-demand neighbors give shortest paths on a uniform grid without an adjacency list. |
| Predecessor reconstruction | `BfsPathfinder::FindPath()` | The parent of each discovery is enough to rebuild a route after search. |
| Finite-state machine | `Guard::Update()` | The current state and visibility determine which behavior runs this frame. |
| Path caching | `Guard::Navigate()` | Keeping the last planned target avoids recomputing an unchanged route every frame. |
| Linear minimum search | `Game::AttackWithKnife()` | One pass finds the nearest eligible guard without sorting or copying the collection. |
| Spatial geometry and grid traversal | `Player::Update()`, `TileMap::CircleHitsWall()`, `TileMap::RayDistance()`, `Guard::CanSee()` | Nearby tile checks resolve movement and boundary traversal tests visibility. |

## 14. Debugging tips and limitations

- Press **F3** to see world position and tile coordinates, guard state text, waypoint rings, and the remaining planned path. A path line can still appear during suspicion because the guard keeps its cached route while paused.
- For startup errors, inspect the message from `main()`. The map must be beside the executable under `assets/maps/`. A shifted map row, bad character, invalid object placement, or disconnected hardcoded patrol leg can stop startup before the window opens. Use the direct VS Code task for the current build.
- For movement issues, inspect each proposed axis separately in `Player::Update()`. Check the world position, the tile bounds that surround the circle, and the nearest-point calculation before adjusting the step size.
- For false detection, inspect `distance`, `dot`, and the result of `RayDistance()` in that order. A rendered triangle may straddle a wall edge because cone rendering samples rays. The player's detection test uses a separate ray aimed at the center.
- For a guard that stops, inspect `state_`, `tile_`, `stepping_`, `plannedTarget_`, `pathIndex_`, and `path_`. The guard pauses during suspicion and replans at tile centers. An unreachable unchanged goal stays cached. Opening a door increments the map revision and makes guards retry at their next tile center.
- BFS optimizes tile steps. It assigns equal cost to straight steps and turns, and uses east, south, west, north to break ties. Guard movement consumes at most one tile-center segment per frame and discards leftover distance after reaching that center.
- For a missed hit, inspect center distance, `IsAlive()`, `HasClearLine()`, and the relevant cooldown. A visible strike effect can represent a missed knife swing or a guard swing during player immunity.
- The world collision checks cover walls. Player and guard circles can overlap. The escape objective remains display text, including after all guards are eliminated.

## 15. Viva questions and answers

**Why use BFS for guard routes?** All legal tile steps have the same cost. FIFO expansion discovers tiles in nondecreasing step count, so the first path to the target is a shortest one.

**Why mark a tile when it is enqueued?** Another expanded tile might otherwise enqueue the same neighbor before its first pop. Early marking preserves the one-entry-per-tile bound and makes a linear queue of size `width * height` sufficient.

**How does the parent array also record visits?** A value of `-1` means undiscovered. Every enqueued tile gets a valid parent, with the start pointing to itself.

**Why is an empty path sometimes successful?** A valid start equal to the destination requires zero steps. The return value distinguishes that from invalid or unreachable input.

**Why does diagonal input need normalization?** Adding two perpendicular unit input components gives a vector of length `sqrt(2)`. Dividing by that length makes movement speed consistent in every direction.

**Why test X and Y separately?** An obstacle may block only one motion component. Accepting the other component lets the actor slide parallel to the wall.

**What is the difference between `tile_` and `WorldTile(position_)` for a guard?** `tile_` records the last center reached. The continuous position can cross into the next tile before reaching its center, so replanning waits until the segment finishes.

**How do walls block sight at corners?** When vertical and horizontal boundary crossings have nearly equal distances, `RayDistance()` checks both side cells and the diagonal cell. A wall on either side blocks the ray.

**Why does a guard forget suspicion but remember a chase?** Suspicion requires continuous visibility for the threshold. During a chase the guard preserves `lastKnown_` when sight breaks so it has a concrete place to investigate.

**Why does the camera use an exponential blend?** Its remaining distance decreases based on elapsed time, which keeps the response comparable at different frame rates. The clamp keeps the camera within the map's valid viewing range.

**How do you find the nearest valid knife target without sorting?** Keep the best distance and a pointer to its guard. Update them only when a living guard is closer, within range, and reachable through a clear segment.

**Why measure direction from the guard to the player?** The attack's side is relative to the guard's facing. A negative dot product means the player is toward the guard's rear.

**Why use both a guard cooldown and player immunity?** A cooldown limits one guard's swing rate. Player immunity prevents several guards from removing multiple HP in the same instant.

**How does the slowdown end when multiple guards are present?** The game combines their chase states. The exposure timer starts counting down only when none of the living guards is chasing.

**Why store crates separately?** Their occupancy can affect the same tile query as walls while the architectural grid and background stay intact. A single blocked-cell query keeps movement, paths, and sight consistent.

## Milestone 4B.1 access and door lookup

- `Game::CardIndex(TilePosition tile) const` uses the authored coordinates to identify Card A or B. `Game::UpdateAccess(float deltaTime)` picks up an uncollected card within 24 world units along a clear line. It checks E against the nearest reachable closed door or terminal within 68 units. `cards_[2]` stays set after unlocking, and rendering hides collected cards and open-door sprites. Access feedback lasts two seconds.
- `TileMap::ConfigureDoors()` inserts the two keys for each `FacilityDoor` into `DoorHash`. Each record has an ID, two tiles, required cards, lockdown, and one open flag. `DoorAt(TilePosition tile) const` retrieves a zero-based index or -1, and `Door(int index) const` exposes the shared record to the game.
- `TileMap::NearbyDoor(Vector2 position, float range, float* distance) const` checks nearby tile keys, skips open doors, and traces to each candidate center. The ray ignores only the candidate doorway. Permanent walls, crates, and other closed doors still obstruct it. The shared `IsBlocked(int column, int row, int ignoredDoor) const` handles all tile checks. Ordinary movement, vision, and melee ignore no door.
- `TileMap::OpenDoor(int index, bool hasA, bool hasB, bool lockdownDisabled)` enforces access and increments the revision when the shared flag changes. `WalkabilityRevision() const` lets guards replan at tile centers when a route may have changed. `ResetDoors()` closes doors for a new round and leaves the hash nodes in place. `Game::ResetRound()` resets cards and feedback and recreates the guards.

The hash key is `row * Width() + column`. `DoorHash::Insert(int key, int doorIndex)` prepends a new node to bucket `key % 17` after checking for a duplicate. `DoorHash::Find(int key) const` follows the bucket's linked list until the full key matches or the chain ends. Different keys that land in the same bucket are a **collision**. Each linked list keeps those keys distinct. Search and insertion cost O(chain length), typically O(1) for the facility's small index and O(n) if all keys collide. Space and `DoorHash::Clear()` time are O(17 + n). The constructor zero-initializes the buckets, the destructor clears all nodes, and copying is disabled to preserve single ownership.

**Why do both doorway tiles open together?** Their hash nodes point to the same record. Only that record stores the open flag.

**Why replan at a tile center after opening?** It keeps the current movement segment intact and lets BFS start from the next known tile once the guard reaches it.

## Milestone 4B.2 terminals and lockdown

`Game::TerminalAt(TilePosition tile) const` identifies the three authored locations when loading placements and scanning nearby terminals. `Game::UpdateAccess(float deltaTime)` uses the same 68-unit radius for doors and terminals. It compares the distance returned by `TileMap::NearbyDoor()` with visible terminal distances, keeps the closer target, and favors a door on an exact tie. Terminal rays use `HasClearLine()`, so walls, crates, and closed doors block interaction.

`activeTerminal_` selects the screen. Two boolean flags record whether the Servers and Security logs were discovered. Control requires both cards but accepts the correct password without either clue flag. `Game::UpdateTerminal()` drains raylib's character queue, accepts only ASCII digits, and caps the null-terminated input at four characters. The comparison uses text to preserve the leading zero in 0742. Backspace deletes one digit and Enter submits. Incorrect input is cleared and can be retried. A correct submission sets `lockdownDisabled_` once. The final exit stays solid until a successful E interaction.

`Game::DrawTerminal() const` draws the screen-space overlay and instructions. `Game::CloseTerminal()` clears password input and errors when Escape closes the overlay or the player loses health. `Game::Update()` routes Escape itself so that the same press cannot close the terminal and quit. It gates movement, interactions, and knife input while the overlay is open. `Player::Update()` still ticks damage immunity, and guards and exposure continue updating. Restart clears the clues, override, terminal, password, and messages alongside the existing round state.

**Why store the override as text?** The leading zero is part of the four-digit input. A character buffer makes it easy to enforce the length and handle Backspace.

**Why do guard attacks still work while typing?** Terminal input changes only which player actions are processed. The same guard, damage, and exposure updates still run.

## Milestone 4B.3 countdown and mission results

`MissionDuration` is 360 seconds. `Game::Update(float deltaTime)` subtracts elapsed seconds only while the mission is active, even during terminal interaction. It clamps timeout to zero and ends the mission before resolving a pending exit. `Game::SecondsRemaining() const` rounds up so 00:00 appears only at expiry. `Game::DrawTimer() const` formats minutes and seconds with division and remainder and uses yellow at 60 seconds and red at 30.

`Game::UpdateAccess(float deltaTime)` uses the same nearest-door and clear-line checks for the final exit. It requires both cards and the completed override, then records an exit request. After guard attacks resolve, `Game::Update()` opens both exit tiles through their shared door record and calls `EndMission(Victory)`. If time or health reaches zero in that update, defeat takes priority and the exit stays closed.

`Game::EndMission(MissionState result)` closes the terminal and clears the interaction target. Active updates stop, so `Game::DrawMissionResult() const` reads the final clock, actual eliminated guards, health, and cards directly. Time taken displays completed seconds, so it sums with the rounded time remaining to 06:00. Defeat distinguishes elimination from timeout. R resets mission state and clock along with the existing round state. Loaded textures and the door index persist through restart.

**Why queue an exit request instead of winning immediately?** Guard damage still has to resolve for that frame. Waiting lets an otherwise lethal hit take priority without briefly opening the exit.

## Milestone 4C live minimap

`Game::DrawMinimap() const` draws in screen space after `EndMode2D()`. It uses five pixels per tile, giving a 250 by 175 pixel content area. The origin is the facility's top-left, with columns increasing right and rows increasing down. World positions are multiplied by `5 / 48` before the panel's screen offset is added. The player marker uses `Player::Facing() const` for its direction line. The camera outline converts the camera bounds from world units and clamps them to the minimap rectangle.

The minimap reads the same state as gameplay every time it draws. Walls use `TileMap::IsWall()`, crates use `TileMap::HasCrate(int column, int row) const`, and shared door records supply red closed doors and ordinary floor for open ones. Card markers disappear on collection. Teal dots mark terminals. The exit changes from muted to bright green when both cards and the override are ready. F3 adds living guards and moves the debug readout below the minimap. Draw calls use the existing grid, actor and object data without a separate map or texture.

M toggles the panel. It hides during terminal and results screens. Restart restores the map markers and keeps the current visibility preference.

**Why use world positions for the player marker?** World positions let the marker move smoothly within a tile while the minimap keeps a fixed orientation.

## Milestone 5A alarm and scheduled events

`EventHeap` stores up to 32 `ScheduledEvent` structs in an array. Earlier mission times come first, and the sequence number breaks ties in insertion order. The root at index 0 is the next event. For a node at index i, its parent is (i - 1) / 2 and its children are 2i + 1 and 2i + 2. `Insert(EventType type, double time)` appends an event and bubbles it up. `ExtractMin(ScheduledEvent& event)` moves the last entry to the root and sifts it down by swapping with the earlier child. `Peek(ScheduledEvent& event) const` reads the root. Empty peeks and extracts and full inserts return false. `Clear()` resets both the size and sequence counter. Peek is O(1), insert and extract are O(log n), and the backing array uses O(capacity) space.

`Game::StartAlarm(double elapsed)` checks that both events fit before it changes state. Only a guard that newly enters Chase during its detection update can trigger an alarm. The alarm records the player's tile and schedules one reinforcement at +8 seconds and expiry at +15 seconds. `Game::ProcessEvents(double elapsed)` drains every due event in priority order and begins a 30-second cooldown at expiry. The elapsed clock comes from the mission countdown, so it advances during terminals and freezes when the mission ends.

`Game::CanSpawnAt(TilePosition tile) const` rejects blocked cells, authored props and spawns, nearby players or living guards, and cells intersecting the camera viewport. `Game::SpawnReinforcement()` tries three fixed entry tiles and gives each new guard a two-point patrol. If all entries are unsafe, the heap schedules up to three one-second retries. After three successful spawns, further detections cannot start an alarm.

`Guard::Investigate(TilePosition target, const TileMap& map, BfsPathfinder& pathfinder)` uses BFS to seed the existing Search state with the snapshot taken at the alarm. If BFS finds no route, the new guard stays on patrol. After spawning it uses ordinary vision, motion, and melee. It counts toward the results total, and F3 shows its path and minimap marker. `Game::DrawAlarm() const` shows the pending time and notifications below the mission timer. Restart clears the heap and alarm state and recreates the seven initial guards.

**Why use a min-heap for delayed events?** It keeps the next due event at the root, so the game can check one entry each frame and process only events whose time has arrived.

**What happens when two events have the same time?** The smaller insertion sequence wins, so the order is deterministic.

## Milestone 5B.1 laser guards and projectiles

`GuardType` distinguishes five initial `MELEE` guards from two `LASER` guards. Reinforcements default to melee. Both types use the same two HP, knife rules, detection cone, suspicion timer, and BFS states. Laser patrol and chase movement use 85 percent of melee speed. The laser range is 240 units, while starting or completing a shot also requires the existing vision check, which has a 210-unit range.

`Guard::UpdateLaserAttack(float deltaTime, Vector2 player, const TileMap& map)` starts a 0.6-second charge after detection if the target is visible and the muzzle line is clear. It stops movement and locks direction. The guard cancels if visibility or the muzzle line is lost, it dies, or the mission ends. When the guard fires, `TakeLaserShot(Vector2& origin, Vector2& direction)` hands the locked direction to `Game` once and a two-second cooldown begins. A target that moves off the firing line can evade the shot even when still visible. Laser guards use ranged attacks exclusively.

`Guard::LaserMuzzle(bool firing) const` rotates the current pose offset around the sprite center. The charge uses (17.8, 10.3), while the aim line and projectile use (16.4, 10.9). The five extra textures load once from `assets/environment_expansion/png/`, including its `effects/` subdirectory. F3 shows each guard type, laser aim and timing, and the active projectile count.

`Game::SpawnProjectile(Vector2 origin, Vector2 direction)` looks for an inactive slot in the 32-entry array. A full array drops the shot safely. Projectiles travel at 480 world units per second for at most one second. `Game::UpdateProjectiles(float deltaTime)` tests the whole movement segment. The tile ray finds the first obstacle and a ray-circle intersection finds the first player contact. The nearer hit wins, and an obstacle wins a tie. This prevents tunnelling across tiles and through a stationary player during a long frame. The hit consumes the projectile, even during immunity. `Game::DamagePlayer(int damage)` shares terminal closure and defeat handling with melee, before escape is resolved.

`Game::DrawLaserCombat() const` draws the shots, charge, and short flash or impact effects. Effects reuse a separate fixed array via `AddLaserEffect(Vector2 position, float angle, bool flash)`. Fired shots are independent of their guard. Results freeze the arrays, and `ClearLaserCombat()` empties them on restart.

For projectile capacity P, effect capacity E, and at most B tile boundaries per shot, worst-case updates take O(E + P * (1 + B + E)) time. A hit may scan for a free effect slot. The two arrays use O(P + E) space, and both capacities are fixed at 32.

**Why compare hit distances?** A swept segment may cross both a wall and the player in one update. Comparing entry distances lets solid cover stop the shot before player damage.

## Milestone 5B.2 character animations

`CharacterFrames` stores three walk frames, three attack or shoot frames, and two death frames per character type. `Game::LoadAnimationArtwork(const std::filesystem::path& directory)` validates all 24 PNGs at 40 by 40. Guards of the same type share those texture handles. An invalid frame logs a warning and uses that character's idle texture. Shutdown unloads loaded frames once, and restarting reuses them. Every frame rotates about (20, 20).

Every `Player` and `Guard` owns a `CharacterAnimation` with pose, frame, elapsed time, and completion flag. `Advance(float deltaTime, float attackFrameTime)` advances by seconds. Walking wraps a 440 ms cycle with frame order 1, 2, 3, 2. `SetMovement(bool moved)` chooses walk only when position changed after collision and preserves an unfinished action. Death has the highest priority, then combat actions, then walk or idle. One-shot actions finish at a fixed duration, and death holds its last frame. Updates and texture lookups are O(1) per actor, and state storage is O(number of actors) with 24 shared frame textures.

- Accepted knives call `Player::StartKnifeAnimation(Vector2 direction)`, with 90 ms per frame. The supplied slash displays during frame 2 at rotated offset (12, 4). Damage still happens immediately on the accepted attack. Melee attacks likewise start a 110 ms per frame sequence from the existing attack event.
- The actual laser charge holds shoot frame 1. On a successful projectile spawn, `Guard::PlayLaserShot()` shows frame 2 for 80 ms and frame 3 for 150 ms. The muzzle flash is six units ahead of the frame-2 muzzle. Cancelling charge never plays the firing pose.
- A lethal hit starts death frames at 180 ms each. Fallen guards are drawn under props and have no gameplay effects. For lethal player defeat, `Player::AdvanceAnimation(float deltaTime)` advances only the collapse before results. Timeouts display results immediately. Restart clears actor playback and effects alongside gameplay state.

**Why does the strike frame leave health alone?** Gameplay starts the animation after accepting the attack, so a skipped or repeated visual frame cannot duplicate damage.
