#include "Guard.h"
#include "Artwork.h"
#include "BfsPathfinder.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Validate every leg of the patrol loop, including the last leg back to the start.
Guard::Guard(const TileMap& map, BfsPathfinder& pathfinder,
             std::initializer_list<TilePosition> patrol, GuardType type)
    : patrol_(patrol), type_(type) {
    if (patrol_.size() < 2) {
        throw std::invalid_argument("A guard patrol needs at least two waypoints.");
    }

    tile_ = patrol_.front();
    position_ = TileMap::TileCenter(tile_);
    path_.reserve(map.Width() * map.Height());
    for (std::size_t i = 0; i < patrol_.size(); ++i) {
        const TilePosition start = patrol_[i];
        const TilePosition destination = patrol_[(i + 1) % patrol_.size()];
        if (!pathfinder.FindPath(map, start, destination, path_)) {
            throw std::invalid_argument("Guard patrol waypoints must be connected floor tiles.");
        }
        if (i == 0 && !path_.empty()) {
            const TilePosition firstStep = path_.front();
            facingAngle_ = std::atan2(static_cast<float>(firstStep.row - tile_.row),
                                      static_cast<float>(firstStep.column - tile_.column));
        }
    }
    path_.clear();
}

// Reject targets outside the cone before tracing a ray through the map.
bool Guard::CanSee(Vector2 player, const TileMap& map) const {
    const Vector2 offset{player.x - position_.x, player.y - position_.y};
    const float distance = std::hypot(offset.x, offset.y);
    if (distance > VisionRange) {
        return false;
    }
    if (distance == 0.0f) {
        return true;
    }

    const Vector2 direction{offset.x / distance, offset.y / distance};
    const float dot = std::cos(facingAngle_) * direction.x + std::sin(facingAngle_) * direction.y;
    return dot >= std::cos(HalfVisionAngle) &&
           map.RayDistance(position_, direction, distance) >= distance;
}

// Process this frame's visibility before taking the resulting state's action.
void Guard::Update(float deltaTime, Vector2 player, const TileMap& map, BfsPathfinder& pathfinder) {
    UpdateWithTarget(deltaTime, &player, map, pathfinder);
}

void Guard::UpdateWithTarget(float deltaTime, const Vector2* player, const TileMap& map, BfsPathfinder& pathfinder) {
    animation_.Advance(deltaTime, 0.11f);
    if (!IsAlive()) {
        return;
    }
    const Vector2 before = position_;
    if (attackCooldown_ > 0.0f) {
        attackCooldown_ -= deltaTime;
    }
    if (attackEffect_ > 0.0f) {
        attackEffect_ -= deltaTime;
    }
    const bool visible = player != nullptr && CanSee(*player, map);
    if (visible) {
        lastKnown_ = TileMap::WorldTile(*player);
        if (state_ == State::Patrol || state_ == State::Suspicious) {
            suspicion_ += deltaTime;
            state_ = suspicion_ >= SuspicionDuration ? State::Chase : State::Suspicious;
        } else {
            state_ = State::Chase;
        }
    } else {
        if (state_ == State::Suspicious) {
            suspicion_ = 0.0f;
            state_ = State::Patrol;
        } else if (state_ == State::Chase) {
            suspicion_ = 0.0f;
            searchTime_ = 0.0f;
            state_ = State::Search;
        }
    }

    float movementFactor = 1.0f;
    if (type_ == LASER) {
        if (!visible) {
            CancelLaserCharge();
        } else if (UpdateLaserAttack(deltaTime, *player, map)) {
            return;
        }
        movementFactor = LaserMovementFactor;
    }
    const float patrolDistance = PatrolSpeed * deltaTime * movementFactor;
    switch (state_) {
        case State::Patrol:
            if (Navigate(patrol_[waypoint_], patrolDistance, map, pathfinder) == Navigation::Arrived) {
                waypoint_ = (waypoint_ + 1) % patrol_.size();
            }
            break;
        case State::Suspicious:
            break;
        case State::Chase:
            Navigate(lastKnown_, ChaseSpeed * deltaTime * movementFactor, map, pathfinder);
            break;
        case State::Search:
            if (Navigate(lastKnown_, patrolDistance, map, pathfinder) != Navigation::Moving) {
                facingAngle_ = std::remainder(facingAngle_ + PI * deltaTime, 2.0f * PI);
                searchTime_ += deltaTime;
                if (searchTime_ >= SearchDuration) {
                    state_ = State::Return;
                }
            }
            break;
        case State::Return:
            if (Navigate(patrol_[waypoint_], patrolDistance, map, pathfinder) == Navigation::Arrived) {
                waypoint_ = (waypoint_ + 1) % patrol_.size();
                state_ = State::Patrol;
            }
            break;
    }
    animation_.SetMovement(position_.x != before.x || position_.y != before.y);
}

GuardType Guard::Type() const { return type_; }

bool Guard::IsCharging() const { return IsAlive() && chargeTime_ > 0.0f; }

Vector2 Guard::LaserDirection() const { return lockedDirection_; }

Vector2 Guard::LaserMuzzle(bool firing) const {
    // The charge and fire poses use different muzzle offsets from the sprite center.
    const float forward = firing ? 16.4f : 17.8f;
    const float sideways = firing ? 10.9f : 10.3f;
    return {position_.x + lockedDirection_.x * forward - lockedDirection_.y * sideways,
            position_.y + lockedDirection_.y * forward + lockedDirection_.x * sideways};
}

void Guard::CancelLaserCharge() {
    chargeTime_ = 0.0f;
    laserShotReady_ = false;
    if (animation_.pose == CharacterPose::Charge) {
        animation_.Start(CharacterPose::Idle);
    }
}

void Guard::PlayLaserShot() { animation_.Start(CharacterPose::Shoot); }

bool Guard::TakeLaserShot(Vector2& origin, Vector2& direction) {
    if (!IsAlive() || !laserShotReady_) {
        return false;
    }
    laserShotReady_ = false;
    origin = LaserMuzzle(true);
    direction = lockedDirection_;
    return true;
}

// Lock the direction once, then check visibility again without retargeting.
bool Guard::UpdateLaserAttack(float deltaTime, Vector2 player, const TileMap& map) {
    const float dx = player.x - position_.x;
    const float dy = player.y - position_.y;
    const float distance = std::hypot(dx, dy);
    if (!IsChasing() || distance > LaserRange || !CanSee(player, map)) {
        CancelLaserCharge();
        return false;
    }
    if (IsCharging()) {
        const Vector2 muzzle = LaserMuzzle(true);
        if (!map.HasClearLine(muzzle, player)) {
            CancelLaserCharge();
            return false;
        }
        chargeTime_ -= deltaTime;
        if (chargeTime_ <= 0.0f) {
            chargeTime_ = 0.0f;
            attackCooldown_ = LaserCooldown;
            laserShotReady_ = true;
            animation_.Start(CharacterPose::Idle);
        }
        return true;
    }
    if (attackCooldown_ > 0.0f) {
        return false;
    }
    float angle = facingAngle_;
    if (distance > 0.0f) {
        angle = std::atan2(dy, dx);
        if (distance > 22.0f) {
            angle -= std::asin(10.9f / distance);
        }
    }
    lockedDirection_ = {std::cos(angle), std::sin(angle)};
    const Vector2 muzzle = LaserMuzzle(true);
    if (!map.HasClearLine(position_, muzzle) || !map.HasClearLine(muzzle, player)) {
        return false;
    }
    facingAngle_ = angle;
    chargeTime_ = LaserChargeDuration;
    animation_.Start(CharacterPose::Charge);
    return true;
}

void Guard::Investigate(TilePosition target, const TileMap& map, BfsPathfinder& pathfinder) {
    if (pathfinder.FindPath(map, tile_, target, path_)) {
        lastKnown_ = target;
        plannedTarget_ = target;
        plannedRevision_ = map.WalkabilityRevision();
        pathIndex_ = 0;
        searchTime_ = 0.0f;
        state_ = State::Search;
    }
}

// Follow one tile-center segment at a time so replanning starts at a known tile.
Guard::Navigation Guard::Navigate(TilePosition target, float distance, const TileMap& map,
                                  BfsPathfinder& pathfinder) {
    if (!stepping_ && (target != plannedTarget_ || plannedRevision_ != map.WalkabilityRevision())) {
        pathfinder.FindPath(map, tile_, target, path_);
        pathIndex_ = 0;
        plannedTarget_ = target;
        plannedRevision_ = map.WalkabilityRevision();
    }
    if (pathIndex_ == path_.size()) {
        return tile_ == target ? Navigation::Arrived : Navigation::Unreachable;
    }

    const TilePosition next = path_[pathIndex_];
    const Vector2 center = TileMap::TileCenter(next);
    const Vector2 offset{center.x - position_.x, center.y - position_.y};
    const float remaining = std::hypot(offset.x, offset.y);
    const float fraction = std::min(distance / remaining, 1.0f);
    const Vector2 proposed = fraction == 1.0f ? center :
        Vector2{position_.x + offset.x * fraction, position_.y + offset.y * fraction};
    if (map.CircleHitsWall(proposed, Radius)) {
        return Navigation::Unreachable;
    }

    facingAngle_ = std::atan2(offset.y, offset.x);
    position_ = proposed;
    stepping_ = position_.x != center.x || position_.y != center.y;
    if (!stepping_) {
        tile_ = next;
        ++pathIndex_;
    }
    return tile_ == target && !stepping_ ? Navigation::Arrived : Navigation::Moving;
}

Color Guard::AlertColor() const {
    switch (state_) {
        case State::Suspicious: return Color{255, 209, 94, 255};
        case State::Chase: return Color{255, 81, 77, 255};
        case State::Search: return Color{255, 155, 87, 255};
        default: return Color{223, 120, 120, 255};
    }
}

// Join neighboring wall-clipped rays to draw a translucent cone.
void Guard::DrawVision(const TileMap& map) const {
    if (!IsAlive()) {
        return;
    }
    constexpr int slices = 48;
    Color color = AlertColor();
    color.a = state_ == State::Patrol || state_ == State::Return ? 37 : 65;
    Vector2 previous = position_;
    for (int i = 0; i <= slices; ++i) {
        const float angle = facingAngle_ - HalfVisionAngle + 2.0f * HalfVisionAngle * i / slices;
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const float distance = map.RayDistance(position_, direction, VisionRange);
        const Vector2 end{position_.x + direction.x * distance, position_.y + direction.y * distance};
        DrawTriangle(position_, end, previous, color);
        previous = end;
    }
}

void Guard::Draw(Texture2D texture, const CharacterFrames* frames) const {
    if (frames != nullptr) {
        texture = animation_.Texture(*frames, texture);
    }
    if (!IsAlive()) {
        if (texture.id != 0) {
            DrawCenteredSprite(texture, position_, facingAngle_ * RAD2DEG, WHITE);
        }
        return;
    }
    if (attackEffect_ > 0.0f) {
        DrawCircleLines(static_cast<int>(position_.x), static_cast<int>(position_.y),
                        AttackRange, Color{255, 112, 82, 255});
    }
    if (texture.id != 0) {
        float angle = facingAngle_;
        if (animation_.pose == CharacterPose::Shoot) {
            angle = std::atan2(lockedDirection_.y, lockedDirection_.x);
        }
        DrawCenteredSprite(texture, position_, angle * RAD2DEG, WHITE);
    } else {
        DrawCircleV(position_, Radius + 2.0f, AlertColor());
        DrawCircleV(position_, Radius, Color{191, 55, 66, 255});
        const Vector2 tip{position_.x + std::cos(facingAngle_) * 11.0f,
                          position_.y + std::sin(facingAngle_) * 11.0f};
        DrawLineEx(position_, tip, 4.0f, Color{55, 19, 27, 255});
    }

    if (state_ == State::Suspicious) {
        DrawText("?", static_cast<int>(position_.x) - 4, static_cast<int>(position_.y) - 34, 16, AlertColor());
    } else if (state_ == State::Chase) {
        DrawText("!", static_cast<int>(position_.x) - 2, static_cast<int>(position_.y) - 34, 16, AlertColor());
    }
}

const char* Guard::StateName() const {
    switch (state_) {
        case State::Patrol: return "PATROL";
        case State::Suspicious: return "SUSPICIOUS";
        case State::Chase: return "CHASE";
        case State::Search: return "SEARCH";
        case State::Return: return "RETURN";
    }
    return "";
}

void Guard::DrawDebug() const {
    if (!IsAlive()) {
        return;
    }
    const Color color = AlertColor();
    Vector2 previous = position_;
    for (std::size_t i = pathIndex_; i < path_.size(); ++i) {
        const Vector2 center = TileMap::TileCenter(path_[i]);
        DrawLineEx(previous, center, 2.0f, color);
        previous = center;
    }
    for (const TilePosition waypoint : patrol_) {
        const Vector2 center = TileMap::TileCenter(waypoint);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), 7.0f, color);
    }
    DrawText(StateName(), static_cast<int>(position_.x) + 20, static_cast<int>(position_.y) - 6, 12, color);
    if (type_ == LASER) {
        const char* label = "LASER";
        if (IsCharging()) {
            label = "LASER CHARGING";
        }
        DrawText(TextFormat("%s %.2fs CD %.2fs", label, chargeTime_, std::max(attackCooldown_, 0.0f)),
                 static_cast<int>(position_.x) + 20, static_cast<int>(position_.y) + 10, 12, color);
        if (IsCharging()) {
            DrawText(TextFormat("Aim %.2f, %.2f", lockedDirection_.x, lockedDirection_.y),
                     static_cast<int>(position_.x) + 20, static_cast<int>(position_.y) + 24, 12, color);
        }
    } else {
        DrawText("MELEE", static_cast<int>(position_.x) + 20, static_cast<int>(position_.y) + 10, 12, color);
    }
    DrawText(TextFormat("%s %d  %.2fs", animation_.Name(), animation_.frame + 1, animation_.elapsed),
             static_cast<int>(position_.x) + 20, static_cast<int>(position_.y) + 38, 12, color);
}

Vector2 Guard::Position() const { return position_; }

void Guard::SetFacing(float degrees) { facingAngle_ = degrees * DEG2RAD; }

bool Guard::IsAlive() const { return health_ > 0; }

bool Guard::IsChasing() const { return IsAlive() && state_ == State::Chase; }

// Project the guard-to-attacker direction onto the guard's forward direction.
Guard::AttackSide Guard::ClassifyAttack(Vector2 attacker) const {
    const Vector2 offset{attacker.x - position_.x, attacker.y - position_.y};
    const float distance = std::hypot(offset.x, offset.y);
    if (distance == 0.0f) {
        return AttackSide::Front;
    }
    const float dot = (std::cos(facingAngle_) * offset.x +
                       std::sin(facingAngle_) * offset.y) / distance;
    if (dot <= -0.5f) {
        return AttackSide::Rear;
    }
    if (dot >= 0.5f) {
        return AttackSide::Front;
    }
    return AttackSide::Side;
}

// Surviving a knife hit alerts the guard and turns it toward the attacker.
void Guard::TakeKnifeHit(Vector2 attacker) {
    if (!IsAlive()) {
        return;
    }
    if (ClassifyAttack(attacker) == AttackSide::Rear) {
        health_ = 0;
        CancelLaserCharge();
        animation_.Start(CharacterPose::Death);
        return;
    }
    --health_;
    if (!IsAlive()) {
        CancelLaserCharge();
        animation_.Start(CharacterPose::Death);
    }
    if (IsAlive()) {
        const float dx = attacker.x - position_.x;
        const float dy = attacker.y - position_.y;
        if (!IsCharging() && (dx != 0.0f || dy != 0.0f)) {
            facingAngle_ = std::atan2(dy, dx);
        }
        lastKnown_ = TileMap::WorldTile(attacker);
        state_ = State::Chase;
    }
}

// A swing starts the guard's cooldown even when the player is immune.
bool Guard::TryAttack(Vector2 player, const TileMap& map) {
    if (type_ != MELEE || !IsChasing() || attackCooldown_ > 0.0f) {
        return false;
    }
    const float distance = std::hypot(player.x - position_.x, player.y - position_.y);
    if (distance > AttackRange || !map.HasClearLine(position_, player)) {
        return false;
    }
    attackCooldown_ = AttackCooldown;
    attackEffect_ = 0.18f;
    animation_.Start(CharacterPose::Attack);
    return true;
}
