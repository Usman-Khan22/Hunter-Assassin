#pragma once

#include "raylib.h"

class TileMap;

class Player {
public:
    void Spawn(Vector2 position, float rotation = 90.0f);
    void Update(float deltaTime, const TileMap& map, bool movementEnabled = true);
    void Draw(Texture2D texture = {}) const;
    Vector2 Position() const;
    int Health() const;
    bool IsExposed() const;
    void UpdateExposure(float deltaTime, bool detected);
    void TakeDamage();

    static constexpr int MaxHealth = 3;

private:
    static constexpr float Radius = 15.0f;
    static constexpr float Speed = 220.0f;
    static constexpr float ExposedSpeedFactor = 0.7f;
    static constexpr float ExposureDuration = 1.25f;
    static constexpr float ImmunityDuration = 0.8f;
    Vector2 position_{};
    Vector2 facing_{0.0f, 1.0f};
    int health_ = MaxHealth;
    float immunity_ = 0.0f;
    float exposure_ = 0.0f;
};
