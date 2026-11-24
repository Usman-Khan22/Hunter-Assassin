#include "Guard.h"
#include "BfsPathfinder.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Validate every leg of the patrol loop, including the last leg back to the start.
Guard::Guard(const TileMap& map, BfsPathfinder& pathfinder,
             std::initializer_list<TilePosition> patrol)
    : patrol_(patrol) {
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
    const bool visible = CanSee(player, map);
    if (visible) {
        lastKnown_ = TileMap::WorldTile(player);
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

    const float patrolDistance = PatrolSpeed * deltaTime;
    switch (state_) {
        case State::Patrol:
            if (Navigate(patrol_[waypoint_], patrolDistance, map, pathfinder) == Navigation::Arrived) {
                waypoint_ = (waypoint_ + 1) % patrol_.size();
            }
            break;
        case State::Suspicious:
            break;
        case State::Chase:
            Navigate(lastKnown_, ChaseSpeed * deltaTime, map, pathfinder);
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
}

// Follow one tile-center segment at a time so replanning starts at a known tile.
Guard::Navigation Guard::Navigate(TilePosition target, float distance, const TileMap& map,
                                  BfsPathfinder& pathfinder) {
    if (!stepping_ && target != plannedTarget_) {
        pathfinder.FindPath(map, tile_, target, path_);
        pathIndex_ = 0;
        plannedTarget_ = target;
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

void Guard::Draw() const {
    DrawCircleV(position_, Radius + 2.0f, AlertColor());
    DrawCircleV(position_, Radius, Color{191, 55, 66, 255});
    const Vector2 tip{position_.x + std::cos(facingAngle_) * 11.0f,
                      position_.y + std::sin(facingAngle_) * 11.0f};
    DrawLineEx(position_, tip, 4.0f, Color{55, 19, 27, 255});

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
}
