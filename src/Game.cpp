#include "Game.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>

void Game::Run() {
    Initialize();
    while (!WindowShouldClose()) {
        Update(GetFrameTime());
        Render();
    }
    Shutdown();
}

// Validate the map and patrol routes before opening the game window.
void Game::Initialize() {
    map_.Load(std::filesystem::u8path(GetApplicationDirectory()) / "maps" / "test_map.txt");
    player_.Spawn(map_.Spawn());
    guards_.reserve(2);
    guards_.emplace_back(map_, pathfinder_,
        std::initializer_list<TilePosition>{{7, 2}, {7, 6}, {11, 6}, {11, 2}, {3, 2}});
    guards_.emplace_back(map_, pathfinder_,
        std::initializer_list<TilePosition>{{18, 8}, {23, 8}, {26, 12}, {27, 12}, {27, 5}, {19, 5}});

    InitWindow(ScreenWidth, ScreenHeight, "Hunter's Assassin");
    if (!IsWindowReady()) {
        throw std::runtime_error("Unable to initialize the game window.");
    }
    SetTargetFPS(60);
    camera_.offset = {ScreenWidth / 2.0f, ScreenHeight / 2.0f};
    camera_.zoom = 1.0f;
    camera_.target = ClampCameraTarget(player_.Position());
}

void Game::Update(float deltaTime) {
    player_.Update(deltaTime, map_);
    for (Guard& guard : guards_) {
        guard.Update(deltaTime, player_.Position(), map_, pathfinder_);
    }
    if (IsKeyPressed(KEY_F3)) {
        debugVisible_ = !debugVisible_;
    }

    const Vector2 target = ClampCameraTarget(player_.Position());
    // Exponential interpolation keeps the camera response consistent across frame rates.
    const float blend = 1.0f - std::exp(-8.0f * deltaTime);
    camera_.target.x += (target.x - camera_.target.x) * blend;
    camera_.target.y += (target.y - camera_.target.y) * blend;
    camera_.target = ClampCameraTarget(camera_.target);
}

// Leave room for half the viewport on either side of the camera target.
Vector2 Game::ClampCameraTarget(Vector2 target) const {
    const float width = static_cast<float>(map_.Width() * TileMap::TileSize);
    const float height = static_cast<float>(map_.Height() * TileMap::TileSize);
    const float halfViewWidth = camera_.offset.x / camera_.zoom;
    const float halfViewHeight = camera_.offset.y / camera_.zoom;

    // Center a map axis that is smaller than the viewport.
    return {
        width <= 2.0f * halfViewWidth ? width / 2.0f :
            std::clamp(target.x, halfViewWidth, width - halfViewWidth),
        height <= 2.0f * halfViewHeight ? height / 2.0f :
            std::clamp(target.y, halfViewHeight, height - halfViewHeight)
    };
}

void Game::Render() const {
    BeginDrawing();
    ClearBackground(Color{16, 21, 27, 255});

    BeginMode2D(camera_);
    map_.Draw();
    for (const Guard& guard : guards_) {
        guard.DrawVision(map_);
    }
    if (debugVisible_) {
        map_.DrawGrid();
        for (const Guard& guard : guards_) {
            guard.DrawDebug();
        }
    }
    for (const Guard& guard : guards_) {
        guard.Draw();
    }
    player_.Draw();
    EndMode2D();

    constexpr Color panel{13, 19, 25, 235};
    constexpr int hudY = ScreenHeight - 92;
    DrawRectangle(16, hudY, 278, 76, panel);
    DrawText("HEALTH  3 / 3", 28, hudY + 11, 18, RAYWHITE);
    for (int point = 0; point < 3; ++point) {
        DrawRectangle(174 + point * 22, hudY + 13, 15, 14, Color{67, 223, 189, 255});
    }
    DrawText("Objective: Find an escape", 28, hudY + 42, 18, RAYWHITE);

    if (debugVisible_) {
        const Vector2 position = player_.Position();
        const TilePosition tile = TileMap::WorldTile(position);
        const int x = ScreenWidth - 260;
        DrawRectangle(x, 16, 244, 101, panel);
        DrawText(TextFormat("FPS: %d", GetFPS()), x + 12, 28, 18, RAYWHITE);
        DrawText(TextFormat("World: %.1f, %.1f", position.x, position.y), x + 12, 54, 18, RAYWHITE);
        DrawText(TextFormat("Tile: %d, %d", tile.column, tile.row), x + 12, 80, 18, RAYWHITE);
    }

    EndDrawing();
}

void Game::Shutdown() {
    CloseWindow();
}
