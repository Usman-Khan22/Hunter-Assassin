#pragma once

#include "TileMap.h"

#include <cstddef>
#include <initializer_list>
#include <vector>

class BfsPathfinder;

class Guard {
public:
    Guard(const TileMap& map, BfsPathfinder& pathfinder,
          std::initializer_list<TilePosition> patrol);

    void Update(float deltaTime, Vector2 player, const TileMap& map, BfsPathfinder& pathfinder);
    void DrawVision(const TileMap& map) const;
    void Draw() const;
    void DrawDebug() const;

private:
    enum class State { Patrol, Suspicious, Chase, Search, Return };
    enum class Navigation { Moving, Arrived, Unreachable };

    bool CanSee(Vector2 player, const TileMap& map) const;
    Navigation Navigate(TilePosition target, float distance, const TileMap& map,
                        BfsPathfinder& pathfinder);
    Color AlertColor() const;
    const char* StateName() const;

    static constexpr float Radius = 15.0f;
    static constexpr float PatrolSpeed = 95.0f;
    static constexpr float ChaseSpeed = 165.0f;
    static constexpr float VisionRange = 210.0f;
    static constexpr float HalfVisionAngle = 40.0f * DEG2RAD;
    static constexpr float SuspicionDuration = 0.65f;
    static constexpr float SearchDuration = 2.0f;

    std::vector<TilePosition> patrol_;
    std::vector<TilePosition> path_;
    std::size_t waypoint_ = 1;
    std::size_t pathIndex_ = 0;
    TilePosition tile_{};
    TilePosition plannedTarget_{-1, -1};
    TilePosition lastKnown_{};
    Vector2 position_{};
    float facingAngle_ = 0.0f;
    float suspicion_ = 0.0f;
    float searchTime_ = 0.0f;
    bool stepping_ = false;
    State state_ = State::Patrol;
};
