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
    void ResetRound();
    void Update(float deltaTime);
    void AttackWithKnife();
    void Render() const;
    void Shutdown();
    Vector2 ClampCameraTarget(Vector2 target) const;

    static constexpr int ScreenWidth = 1280;
    static constexpr int ScreenHeight = 720;
    static constexpr float KnifeRange = 52.0f;
    static constexpr float KnifeCooldown = 0.45f;
    TileMap map_;
    Player player_;
    BfsPathfinder pathfinder_;
    std::vector<Guard> guards_;
    Camera2D camera_{};
    bool debugVisible_ = false;
    float knifeCooldown_ = 0.0f;
    float knifeEffect_ = 0.0f;
    Vector2 knifeOrigin_{};
    Vector2 knifeTarget_{};
};
