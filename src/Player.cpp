#include "Player.h"
#include "TileMap.h"

#include <cmath>

void Player::Spawn(Vector2 position) {
    position_ = position;
    facing_ = {0.0f, 1.0f};
}

// Normalize input before scaling by time, then resolve each axis to allow wall sliding.
void Player::Update(float deltaTime, const TileMap& map) {
    const Vector2 input{
        static_cast<float>(IsKeyDown(KEY_D) - IsKeyDown(KEY_A)),
        static_cast<float>(IsKeyDown(KEY_S) - IsKeyDown(KEY_W))
    };
    const float length = std::hypot(input.x, input.y);
    if (length == 0.0f || deltaTime <= 0.0f) {
        return;
    }

    facing_ = {input.x / length, input.y / length};
    const float distance = Speed * deltaTime;
    // Small substeps prevent tunneling and keep corner contact stable.
    const int steps = static_cast<int>(std::ceil(distance / (Radius * 0.25f)));
    const float stride = distance / steps;
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
}

void Player::Draw() const {
    DrawCircleV(position_, Radius + 2.0f, Color{11, 20, 25, 255});
    DrawCircleV(position_, Radius, Color{67, 223, 189, 255});
    const Vector2 tip{position_.x + facing_.x * 11.0f, position_.y + facing_.y * 11.0f};
    DrawLineEx(position_, tip, 4.0f, Color{17, 55, 52, 255});
}

Vector2 Player::Position() const { return position_; }
