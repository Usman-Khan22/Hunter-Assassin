#pragma once

#include "TileMap.h"
#include "CharacterAnimation.h"

#include <cstddef>
#include <initializer_list>
#include <vector>

class BfsPathfinder;

enum GuardType { MELEE, LASER };

class Guard {
public:
    Guard(const TileMap& map, BfsPathfinder& pathfinder,
          std::initializer_list<TilePosition> patrol, GuardType type = MELEE);

    void Update(float deltaTime, Vector2 player, const TileMap& map, BfsPathfinder& pathfinder);
    void UpdateWithTarget(float deltaTime, const Vector2* player, const TileMap& map, BfsPathfinder& pathfinder);
    void DrawVision(const TileMap& map) const;
    void Draw(Texture2D texture = {}, const CharacterFrames* frames = nullptr) const;
    void DrawDebug() const;
    Vector2 Position() const;
    void SetFacing(float degrees);
    bool IsAlive() const;
    bool IsChasing() const;
    void TakeKnifeHit(Vector2 attacker);
    bool TryAttack(Vector2 player, const TileMap& map);
    void Investigate(TilePosition target, const TileMap& map, BfsPathfinder& pathfinder);
    GuardType Type() const;
    bool IsCharging() const;
    Vector2 LaserMuzzle(bool firing = false) const;
    Vector2 LaserDirection() const;
    bool TakeLaserShot(Vector2& origin, Vector2& direction);
    void CancelLaserCharge();
    void PlayLaserShot();
    static constexpr float LaserRange = 240.0f;

private:
    enum class State { Patrol, Suspicious, Chase, Search, Return };
    enum class Navigation { Moving, Arrived, Unreachable };
    enum class AttackSide { Front, Side, Rear };

    bool CanSee(Vector2 player, const TileMap& map) const;
    Navigation Navigate(TilePosition target, float distance, const TileMap& map,
                        BfsPathfinder& pathfinder);
    Color AlertColor() const;
    const char* StateName() const;
    AttackSide ClassifyAttack(Vector2 attacker) const;
    bool UpdateLaserAttack(float deltaTime, Vector2 player, const TileMap& map);

    static constexpr float Radius = 15.0f;
    static constexpr float PatrolSpeed = 95.0f;
    static constexpr float ChaseSpeed = 165.0f;
    static constexpr float VisionRange = 210.0f;
    static constexpr float HalfVisionAngle = 40.0f * DEG2RAD;
    static constexpr float SuspicionDuration = 0.65f;
    static constexpr float SearchDuration = 2.0f;
    static constexpr float AttackRange = 44.0f;
    static constexpr float AttackCooldown = 1.0f;
    static constexpr float LaserMovementFactor = 0.85f;
    static constexpr float LaserChargeDuration = 0.6f;
    static constexpr float LaserCooldown = 2.0f;

    std::vector<TilePosition> patrol_;
    std::vector<TilePosition> path_;
    std::size_t waypoint_ = 1;
    std::size_t pathIndex_ = 0;
    TilePosition tile_{};
    TilePosition plannedTarget_{-1, -1};
    unsigned int plannedRevision_ = 0;
    TilePosition lastKnown_{};
    Vector2 position_{};
    float facingAngle_ = 0.0f;
    float suspicion_ = 0.0f;
    float searchTime_ = 0.0f;
    bool stepping_ = false;
    State state_ = State::Patrol;
    int health_ = 2;
    float attackCooldown_ = 0.0f;
    float attackEffect_ = 0.0f;
    GuardType type_ = MELEE;
    float chargeTime_ = 0.0f;
    bool laserShotReady_ = false;
    Vector2 lockedDirection_{};
    CharacterAnimation animation_;
};
