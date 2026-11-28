#pragma once

#include "raylib.h"
#include "CharacterAnimation.h"

class TileMap;

enum AbilityType { Cloak, Boost, AbilityCount };

struct PlayerAbility {
    int charges;
    float remaining;
    float duration;

    bool Active() const { return remaining > 0.0f; }
};

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
    const PlayerAbility& Ability(AbilityType type) const;
    bool ActivateAbility(AbilityType type);
    bool AddAbilityCharge(AbilityType type);
    void TickAbilities(float deltaTime);
    bool IsInvisible() const;
    void BreakCloak();

    static constexpr int MaxHealth = 3;
    static constexpr float Radius = 15.0f;
    static constexpr int MaxAbilityCharges = 2;

private:
    static constexpr float Speed = 220.0f;
    static constexpr float ExposedSpeedFactor = 0.7f;
    static constexpr float ExposureDuration = 1.25f;
    static constexpr float ImmunityDuration = 0.8f;
    static constexpr float BoostSpeedFactor = 1.5f;
    Vector2 position_{};
    Vector2 facing_{0.0f, 1.0f};
    int health_ = MaxHealth;
    float immunity_ = 0.0f;
    float exposure_ = 0.0f;
    CharacterAnimation animation_;
    Vector2 knifeDirection_{1, 0};
    PlayerAbility abilities_[AbilityCount]{{1, 0.0f, 4.0f}, {1, 0.0f, 3.0f}};
};
