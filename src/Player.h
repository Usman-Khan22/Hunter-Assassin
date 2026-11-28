#pragma once

#include "raylib.h"
#include "CharacterAnimation.h"

class TileMap;

class Player {
public:
    void Spawn(Vector2 position, float rotation = 90.0f);
    void Update(float deltaTime, const TileMap& map, bool movementEnabled = true);
    void Draw(Texture2D texture = {}, const CharacterFrames* frames = nullptr) const;
    Vector2 Position() const;
    Vector2 Facing() const;
    int Health() const;
    bool IsExposed() const;
    void UpdateExposure(float deltaTime, bool detected);
    void TakeDamage(int damage = 1);
    void StartKnifeAnimation(Vector2 direction);
    void AdvanceAnimation(float deltaTime);
    const CharacterAnimation& Animation() const;
    Vector2 KnifeDirection() const;

    static constexpr int MaxHealth = 3;
    static constexpr float Radius = 15.0f;

private:
    static constexpr float Speed = 220.0f;
    static constexpr float ExposedSpeedFactor = 0.7f;
    static constexpr float ExposureDuration = 1.25f;
    static constexpr float ImmunityDuration = 0.8f;
    Vector2 position_{};
    Vector2 facing_{0.0f, 1.0f};
    int health_ = MaxHealth;
    float immunity_ = 0.0f;
    float exposure_ = 0.0f;
    CharacterAnimation animation_;
    Vector2 knifeDirection_{1, 0};
};
