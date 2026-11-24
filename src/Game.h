#pragma once

#include "BfsPathfinder.h"
#include "Guard.h"
#include "Player.h"
#include "TileMap.h"

class Game {
public:
    void Run();

private:
    void Initialize();
    void Update(float deltaTime);
    void Render() const;
    void Shutdown();
    Vector2 ClampCameraTarget(Vector2 target) const;

    static constexpr int ScreenWidth = 1280;
    static constexpr int ScreenHeight = 720;
    TileMap map_;
    Player player_;
    BfsPathfinder pathfinder_;
    std::vector<Guard> guards_;
    Camera2D camera_{};
    bool debugVisible_ = false;
};
