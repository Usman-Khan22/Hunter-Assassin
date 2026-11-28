#include "Player.h"
#include "Artwork.h"
#include "TileMap.h"

#include <cmath>

void Player::Spawn(Vector2 position, float rotation) {
    position_ = position;
    const float angle = rotation * DEG2RAD;
    facing_ = {std::cos(angle), std::sin(angle)};
    health_ = MaxHealth;
    immunity_ = 0.0f;
    exposure_ = 0.0f;
    animation_.Start(CharacterPose::Idle);
    knifeDirection_ = facing_;
}

// Normalize input before scaling by time, then resolve each axis to allow wall sliding.
void Player::Update(float deltaTime, const TileMap& map, bool movementEnabled) {
    AdvanceAnimation(deltaTime);
    if (immunity_ > 0.0f) {
        immunity_ -= deltaTime;
    }
    if (!movementEnabled) {
        animation_.SetMovement(false);
        return;
    }
    const Vector2 input{
        static_cast<float>(IsKeyDown(KEY_D) - IsKeyDown(KEY_A)),
        static_cast<float>(IsKeyDown(KEY_S) - IsKeyDown(KEY_W))
    };
    const float length = std::hypot(input.x, input.y);
    if (length == 0.0f || deltaTime <= 0.0f) {
        animation_.SetMovement(false);
        return;
    }

    facing_ = {input.x / length, input.y / length};
    float distance = Speed * deltaTime;
    if (IsExposed()) {
        distance *= ExposedSpeedFactor;
    }
    // Small substeps prevent tunneling and keep corner contact stable.
    const int steps = static_cast<int>(std::ceil(distance / (Radius * 0.25f)));
    const float stride = distance / steps;
    const Vector2 before = position_;
    for (int step = 0; step < steps; ++step) {
        const Vector2 horizontal{position_.x + facing_.x * stride, position_.y};
        if (!map.CircleHitsWall(horizontal, Radius)) {
            position_.x = horizontal.x;
        }
        const Vector2 vertical{position_.x, position_.y + facing_.y * stride};
        if (!map.CircleHitsWall(vertical, Radius)) {
            position_.y = vertical.y;
        }
    }
    animation_.SetMovement(position_.x != before.x || position_.y != before.y);
}

void Player::Draw(Texture2D texture, const CharacterFrames* frames) const {
    if (frames != nullptr) {
        texture = animation_.Texture(*frames, texture);
    }
    if (texture.id != 0) {
        Color tint = WHITE;
        if (immunity_ > 0.0f) {
            tint = Color{255, 155, 155, 255};
        }
        Vector2 direction = facing_;
        if (animation_.pose == CharacterPose::Attack) {
            direction = knifeDirection_;
        }
        DrawCenteredSprite(texture, position_, std::atan2(direction.y, direction.x) * RAD2DEG, tint);
        return;
    }
    DrawCircleV(position_, Radius + 2.0f, Color{11, 20, 25, 255});
    Color body{67, 223, 189, 255};
    if (immunity_ > 0.0f) {
        body = RAYWHITE;
    }
    DrawCircleV(position_, Radius, body);
    const Vector2 tip{position_.x + facing_.x * 11.0f, position_.y + facing_.y * 11.0f};
    DrawLineEx(position_, tip, 4.0f, Color{17, 55, 52, 255});
}

Vector2 Player::Position() const { return position_; }

Vector2 Player::Facing() const { return facing_; }

int Player::Health() const { return health_; }

bool Player::IsExposed() const { return exposure_ > 0.0f; }

// Refresh while any guard detects the player, then let the slowdown expire.
void Player::UpdateExposure(float deltaTime, bool detected) {
    if (detected) {
        exposure_ = ExposureDuration;
    } else if (IsExposed()) {
        exposure_ -= deltaTime;
    }
}

void Player::TakeDamage(int damage) {
    if (health_ > 0 && immunity_ <= 0.0f && damage > 0) {
        health_ -= damage;
        if (health_ < 0) {
            health_ = 0;
        }
        if (health_ == 0) {
            animation_.Start(CharacterPose::Death);
        }
        immunity_ = ImmunityDuration;
    }
}

void Player::StartKnifeAnimation(Vector2 direction) {
    knifeDirection_ = direction;
    animation_.Start(CharacterPose::Attack);
}

void Player::AdvanceAnimation(float deltaTime) { animation_.Advance(deltaTime, 0.09f); }

const CharacterAnimation& Player::Animation() const { return animation_; }

Vector2 Player::KnifeDirection() const { return knifeDirection_; }
