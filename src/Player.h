#pragma once

#include "raylib.h"

class TileMap;

class Player {
public:
    void Spawn(Vector2 position);
    void Update(float deltaTime, const TileMap& map);
    void Draw() const;
    Vector2 Position() const;

private:
    static constexpr float Radius = 15.0f;
    static constexpr float Speed = 220.0f;
    Vector2 position_{};
    Vector2 facing_{0.0f, 1.0f};
};
